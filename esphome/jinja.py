import logging
import math
import re
import jinja2 as jinja
from jinja2.nativetypes import NativeEnvironment
from esphome.const import VALID_SUBSTITUTIONS_CHARACTERS

TemplateError = jinja.TemplateError
TemplateSyntaxError = jinja.TemplateSyntaxError
TemplateRuntimeError = jinja.TemplateRuntimeError
UndefinedError = jinja.UndefinedError

_LOGGER = logging.getLogger(__name__)
DETECT_JINJA = r"(\$\{)"

# Escape the full set for “rest” of the var name:
rest_chars = re.escape(VALID_SUBSTITUTIONS_CHARACTERS)
# Build a “first‐char” class: same set, but drop digits
first_chars = re.escape(
    "".join(c for c in VALID_SUBSTITUTIONS_CHARACTERS if not c.isdigit())
)

detect_expression_re = re.compile(
    r"\{\$.+?\$\}"  # Block form expression: {$ ... $}
    r"|\$\{[^}]+\}"  # Braced form expression: ${ ... }
    r"|\$["  # $varName (must start with letter or underscore)
    + first_chars
    + r"]["
    + rest_chars
    + r"]*",
    flags=re.MULTILINE,
)

detect_jinja_re = re.compile(
    r"\{\$.+?\$\}"  # Block form expression: {$ ... $}
    r"|\$\{[^}]+\}",  # Braced form expression: ${ ... }
    flags=re.MULTILINE,
)


def has_expression(s: str) -> bool:
    """
    Return True if `s` contains either:
      - {$ ... $} (block expressions)
      - ${ ... }  (inline expressions or vars)
      - $varName  (names starting with letter/_ plus letters/digits/_)
    """
    return detect_expression_re.search(s) is not None


def has_jinja(st):
    return detect_jinja_re.search(st) is not None


jinja = NativeEnvironment(
    trim_blocks=True,
    lstrip_blocks=True,
    block_start_string="{$",
    block_end_string="$}",
    line_comment_prefix="##",
    variable_start_string="${",
    variable_end_string="}",
    undefined=jinja.StrictUndefined,
)
jinja.add_extension("jinja2.ext.do")
jinja.globals["math"] = math  # Inject entire math module


def expand_str(st, vars):
    """
    Evaluates a jinja expression, considering `vars`.
    """
    template = jinja.from_string(st)
    result = template.render(vars)
    return result
