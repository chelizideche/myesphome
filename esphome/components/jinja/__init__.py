from collections import ChainMap
import json
import logging
import math
import re
import jinja2 as jinja
import voluptuous as vol
from jinja2.nativetypes import NativeEnvironment
from esphome.const import CONF_JINJA, VALID_SUBSTITUTIONS_CHARACTERS
import esphome.config_validation as cv

TemplateError = jinja.TemplateError
TemplateSyntaxError = jinja.TemplateSyntaxError
TemplateRuntimeError = jinja.TemplateRuntimeError
UndefinedError = jinja.UndefinedError
Undefined = jinja.Undefined

CODEOWNERS = ["@jpeletier"]
_LOGGER = logging.getLogger(__name__)


def is_jinja_enabled(config):
    return CONF_JINJA in config


def validate_identifier(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Identifier name must not be empty")
    if value[0].isdigit():
        raise cv.Invalid("First character in an identifier cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise cv.Invalid(
                f"Jinja identifier names must only consist of upper/lowercase characters, the underscore and numbers. The character '{char}' cannot be used"
            )
    return value


def _merge_return_into_body(obj):
    """
    Combines the value of "return" into the macro body
    """
    params = obj["parameters"]
    body = obj.get("body", "")
    ret = obj.get("return")

    if ret is not None:
        # wrap the return value
        ret_stmt = f"${{{ret}}}"
        if body:
            body = f"{body}\n{ret_stmt}"
        else:
            body = ret_stmt

    return {"parameters": params, "body": body, "upvalues": obj.get("upvalues", {})}


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional("macros"): cv.Schema(
            {
                validate_identifier: cv.All(
                    {
                        cv.Optional("parameters"): cv.ensure_schema(
                            cv.Schema({validate_identifier: object})
                        ),
                        cv.Optional("upvalues"): dict,
                        cv.Optional("body"): cv.string,
                        cv.Optional("return"): cv.string,
                    },
                    _merge_return_into_body,
                )
            },
            extra=vol.PREVENT_EXTRA,
        ),
        cv.Optional("vars"): cv.ensure_schema({validate_identifier: object}),
    }
)


DETECT_JINJA = r"(\$\{)"
detect_jinja_re = re.compile(
    r"<%.+?%>"  # Block form expression: <% ... %>
    r"|\$\{[^}]+\}",  # Braced form expression: ${ ... }
    flags=re.MULTILINE,
)


def has_jinja(st):
    return detect_jinja_re.search(st) is not None


class JinjaStr(str):
    """
    Wraps a string containing an unresolved Jinja expression,
    storing the variables visible to it when it failed to resolve.
    For example, an expression inside a package, `${ A * B }` may fail
    to resolve at package parsing time if `A` is a local package var
    but `B` is a substitution defined in the root yaml.
    Therefore, we store the value of `A` as an upvalue bound
    to the original string so we may be able to resolve `${ A * B }`
    later in the main substitutions pass.
    """

    def __new__(cls, value: str, upvalues=None):
        obj = super().__new__(cls, value)
        obj.upvalues = upvalues or {}
        return obj

    def __init__(self, value: str, upvalues=None):
        self.upvalues = upvalues or {}


class PythonLiteralEncoder(json.JSONEncoder):
    """
    JSON encoder that translates `null` to None
    """

    def iterencode(self, o, _one_shot=False):
        # stream through the default encoder
        for chunk in super().iterencode(o, _one_shot=_one_shot):
            # swap out any standalone "null"
            yield chunk.replace("null", "None")


class Jinja:
    """
    Wraps a Jinja environment
    """

    def __init__(self, config, context_vars):
        self.env = NativeEnvironment(
            trim_blocks=True,
            lstrip_blocks=True,
            block_start_string="<%",
            block_end_string="%>",
            line_statement_prefix="#",
            line_comment_prefix="##",
            variable_start_string="${",
            variable_end_string="}",
            undefined=jinja.StrictUndefined,
        )
        self.env.add_extension("jinja2.ext.do")
        self.env.globals["math"] = math  # Inject entire math module
        self.context_vars = {**context_vars}

        if CONF_JINJA in config:
            with cv.prepend_path(CONF_JINJA):
                config[CONF_JINJA] = CONFIG_SCHEMA(config[CONF_JINJA])

            jinja_config = config[CONF_JINJA]
            if "vars" in jinja_config:
                self.load_vars(jinja_config["vars"])
            if "macros" in jinja_config:
                self.load_macros(jinja_config["macros"])

        self.env.globals = {**self.env.globals, **self.context_vars}

    def parse_template(self, content, upvalues, imports=None):
        local_env = self.env
        if len(upvalues) > 0:
            local_env = self.env.overlay()
            local_env.globals = ChainMap(upvalues, self.env.globals)
        template = local_env.from_string(content)
        if imports is None:
            # import all symbols
            for symbol_name in dir(template.module):
                if symbol_name.startswith("_"):
                    continue
                self.env.globals[symbol_name] = getattr(template.module, symbol_name)
        else:
            for symbol_name in imports:
                symbol = getattr(template.module, symbol_name)
                if symbol is not None:
                    self.env.globals[symbol_name] = symbol

    def load_vars(self, vars):
        """
        Adds variables only visible to Jinja. Note that substitutions
        have precedence and will override these
        """
        for var_name, value in vars.items():
            if var_name not in self.context_vars:
                self.context_vars[var_name] = value

    def load_macros(self, macros):
        """
        Creates Jinja macros out of a simplified yaml syntax
        """
        for name, macro in macros.items():
            parameters = ", ".join(
                [
                    f"{k}={json.dumps(v, cls=PythonLiteralEncoder)}"
                    for k, v in macro["parameters"].items()
                ]
            )
            macro["upvalues"] = upvalues = {
                **self.context_vars,
                **macro.get("upvalues", {}),
            }
            body = macro["body"]
            self.parse_template(
                f"<% macro {name}({parameters}) %>\n{body}<% endmacro %>",
                upvalues,
                [name],
            )

    def expand(self, content_str):
        """
        Renders a string that may contain Jinja expressions or statements
        Returns the resulting processed string if all values could be resolved.
        Otherwise, it returns a tagged (JinjaStr) string that captures variables
        in scope (upvalues), like a closure for later evaluation.
        """
        result = None
        override_vars = {}
        if isinstance(content_str, JinjaStr):
            # If `value` is already a JinjaStr, it means we are trying to evaluate it again
            # in a parent pass.
            # Hopefully, all required variables are visible now.
            override_vars = content_str.upvalues
        try:
            template = self.env.from_string(content_str)
            result = template.render(override_vars)
            if isinstance(result, Undefined):
                print("" + result)  # force a UndefinedError exception
        except (TemplateSyntaxError, UndefinedError) as err:
            # `content_str` contains a Jinja expression that refers to a variable that is undefined
            # in this scope. Perhaps it refers to a root substitution that is not visible yet.
            # Therefore, return `content_str` as a JinjaStr, which contains the variables
            # that are actually visible to it at this point to postpone evaluation.
            return JinjaStr(content_str, {**self.context_vars, **override_vars}), err

        return result, None
