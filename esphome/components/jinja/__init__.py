import json
import logging
import math
import re
import jinja2 as jinja
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


def validate_module_name(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Module name must not be empty")
    if value[0].isdigit():
        raise cv.Invalid("First character in a module cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise cv.Invalid(
                f"Jinja module names must only consist of upper/lowercase characters, the underscore and numbers. The character '{char}' cannot be used"
            )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional("macros"): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required("name"): validate_module_name,
                    cv.Optional("parameters"): cv.ensure_schema(
                        cv.Schema({validate_module_name: object})
                    ),
                    cv.Required("return"): cv.string,
                }
            )
        ),
        cv.Optional("vars"): cv.ensure_schema(
            cv.Schema({validate_module_name: object})
        ),
        cv.Optional("templates"): cv.ensure_list(cv.string_strict),
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


class VariableResolver(jinja.runtime.Context):
    def resolve_or_missing(self, key):
        if key in self.environment.globals["__vars"]:
            return self.environment.globals["__vars"][key]
        return super().resolve_or_missing(key)


class Jinja:
    def __init__(self, config):
        self.env = NativeEnvironment(
            trim_blocks=True,
            lstrip_blocks=True,
            block_start_string="<%",
            block_end_string="%>",
            line_comment_prefix="##",
            variable_start_string="${",
            variable_end_string="}",
            undefined=jinja.StrictUndefined,
        )
        self.env.add_extension("jinja2.ext.do")
        self.env.globals["math"] = math  # Inject entire math module
        self.env.context_class = VariableResolver

        if CONF_JINJA in config:
            with cv.prepend_path(CONF_JINJA):
                config[CONF_JINJA] = CONFIG_SCHEMA(config[CONF_JINJA])

            jinja_config = config[CONF_JINJA]
            if "vars" in jinja_config:
                self.load_vars(jinja_config["vars"])
            if "macros" in jinja_config:
                self.load_macros(jinja_config["macros"])
            if "templates" in jinja_config:
                self.load_templates(jinja_config["templates"])

    def parse_template(self, content):
        template = self.env.from_string(content)
        for symbol_name in dir(template.module):
            if symbol_name.startswith("_"):
                continue
            self.env.globals[symbol_name] = getattr(template.module, symbol_name)

    def load_macros(self, macros):
        for macro in macros:
            name = macro["name"]
            parameters = ", ".join(
                [f"{k}={json.dumps(v)}" for k, v in macro["parameters"].items()]
            )
            return_value = macro["return"]
            self.parse_template(
                f"<% macro {name}({parameters}) %>${{{return_value}}}<% endmacro %>",
            )

    def load_vars(self, vars):
        for var_name, value in vars.items():
            self.env.globals[var_name] = value

    def load_templates(self, templates):
        for content in templates:
            self.parse_template(content)

    def expand(self, st, vars):
        """
        Evaluates a jinja expression, considering `vars`.
        """
        self.env.globals["__vars"] = vars
        template = self.env.from_string(st)
        result = template.render()
        self.env.globals["__vars"] = {}
        return result
