import logging

from esphome import core
from esphome.config_helpers import Extend, Remove, merge_config
import esphome.config_validation as cv
from esphome.const import CONF_SUBSTITUTIONS, VALID_SUBSTITUTIONS_CHARACTERS
from esphome.jinja import (
    UndefinedError,
    expand_str,
    has_jinja,
    TemplateError,
    TemplateSyntaxError,
    TemplateRuntimeError,
    Undefined,
)
from esphome.yaml_util import ESPHomeDataBase, make_data_base

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)


class ExpressionEvalError(Exception):
    def __init__(self, message, path=None):
        super().__init__(message)
        self.path = path


def validate_substitution_key(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Substitution key must not be empty")
    if value[0] == "$":
        value = value[1:]
    if value[0].isdigit():
        raise cv.Invalid("First character in substitutions cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise cv.Invalid(
                f"Substitution must only consist of upper/lowercase characters, the underscore and numbers. The character '{char}' cannot be used"
            )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        validate_substitution_key: object,
    }
)


async def to_code(config):
    pass


def _expand_substitutions(substitutions, value, path, ignore_missing):
    if "$" not in value:
        return value

    orig_value = value

    i = 0
    while True:
        m = cv.VARIABLE_PROG.search(value, i)
        if not m:
            # No more variable substitutions found. See if the remainder looks like a jinja template
            if has_jinja(value):
                expr_result = None
                try:
                    # Invoke the jinja engine to evaluate the expression.
                    expr_result = expand_str(value, substitutions)
                    if isinstance(expr_result, Undefined):
                        print("" + expr_result)  # force a UndefinedError exception
                    value = expr_result
                except UndefinedError as err:
                    if not ignore_missing and "password" not in path:
                        _LOGGER.warning(
                            "Found '%s' (see %s) which looks like an expression,"
                            " but could not resolve all the variables: %s",
                            value,
                            "->".join(str(x) for x in path),
                            err.message,
                        )
                except (
                    TemplateError,
                    TemplateSyntaxError,
                    TemplateRuntimeError,
                    RuntimeError,
                    ArithmeticError,
                    AttributeError,
                    TypeError,
                ) as err:
                    raise cv.Invalid(
                        f"{type(err).__name__} Error evaluating jinja expression '{value}': {str(err)}."
                        f" See {'->'.join(str(x) for x in path)}",
                        path,
                    )

            break

        i, j = m.span(0)
        name = m.group(1)
        if name.startswith("{") and name.endswith("}"):
            name = name[1:-1]
        if name not in substitutions:
            if not ignore_missing and "password" not in path:
                _LOGGER.warning(
                    "Found '%s' (see %s) which looks like a substitution, but '%s' was "
                    "not declared",
                    orig_value,
                    "->".join(str(x) for x in path),
                    name,
                )
            i = j
            continue

        sub = substitutions[name]

        if i == 0 and j == len(value):
            # The variable spans the whole expression, e.g., "${varName}". Return its resolved value directly
            # to conserve its type.
            value = sub
            break

        tail = value[j:]
        value = value[:i] + str(sub)
        i = len(value)
        value += tail

    # orig_value can also already be a lambda with esp_range info, and only
    # a plain string is sent in orig_value
    if isinstance(orig_value, ESPHomeDataBase):
        # even though string can get larger or smaller, the range should point
        # to original document marks
        return make_data_base(value, orig_value)

    return value


def _substitute_item(substitutions, item, path, ignore_missing):
    if isinstance(item, list):
        for i, it in enumerate(item):
            sub = _substitute_item(substitutions, it, path + [i], ignore_missing)
            if sub is not None:
                item[i] = sub
    elif isinstance(item, dict):
        replace_keys = []
        for k, v in item.items():
            if path or k != CONF_SUBSTITUTIONS:
                sub = _substitute_item(substitutions, k, path + [k], ignore_missing)
                if sub is not None:
                    replace_keys.append((k, sub))
            sub = _substitute_item(substitutions, v, path + [k], ignore_missing)
            if sub is not None:
                item[k] = sub
        for old, new in replace_keys:
            item[new] = merge_config(item.get(old), item.get(new))
            del item[old]
    elif isinstance(item, str):
        sub = _expand_substitutions(substitutions, item, path, ignore_missing)
        if sub != item:
            return sub
    elif isinstance(item, (core.Lambda, Extend, Remove)):
        sub = _expand_substitutions(substitutions, item.value, path, ignore_missing)
        if sub != item:
            item.value = sub
    return None


def do_substitution_pass(config, command_line_substitutions, ignore_missing=False):
    if CONF_SUBSTITUTIONS not in config and not command_line_substitutions:
        return

    substitutions = config.get(CONF_SUBSTITUTIONS)
    if substitutions is None:
        substitutions = command_line_substitutions
    elif command_line_substitutions:
        substitutions = {**substitutions, **command_line_substitutions}
    with cv.prepend_path("substitutions"):
        if not isinstance(substitutions, dict):
            raise cv.Invalid(
                f"Substitutions must be a key to value mapping, got {type(substitutions)}"
            )

        replace_keys = []
        for key, value in substitutions.items():
            with cv.prepend_path(key):
                sub = validate_substitution_key(key)
                if sub != key:
                    replace_keys.append((key, sub))
                substitutions[key] = value
        for old, new in replace_keys:
            substitutions[new] = substitutions[old]
            del substitutions[old]

    config[CONF_SUBSTITUTIONS] = substitutions
    # Move substitutions to the first place to replace substitutions in them correctly
    config.move_to_end(CONF_SUBSTITUTIONS, False)
    _substitute_item(substitutions, config, [], ignore_missing)
