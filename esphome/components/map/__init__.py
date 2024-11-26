from esphome.const import CONF_ID, CONF_FROM, CONF_TO

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.core import CORE
from esphome.cpp_generator import MockObj, VariableDeclarationExpression, add_global

CODEOWNERS = ["@clydebarrow"]

map_ = cg.std_ns.class_("map")

CONF_ENTRIES = "entries"
CONF_CLASS = "class"

class IndexType:
    def __init__(self, validator, data_type, conversion):
        self.validator = validator
        self.data_type = data_type
        self.conversion = conversion


INDEX_TYPES = {
    "int": IndexType(cv.int_, cg.int_, int),
    "string": IndexType(cv.string, cg.std_string, str),
}

def to_schema(value):
    return cv.Any(
        cv.one_of(*INDEX_TYPES, lower=True),
        cv.Schema({cv.Required(CONF_CLASS): cv.one_of(*MockObj.known_classes.keys())})
    )(value)


BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(map_),
        cv.Required(CONF_FROM): cv.one_of(*INDEX_TYPES, lower=True),
        cv.Required(CONF_TO): to_schema,
        cv.Required(CONF_ENTRIES): dict,
    }
)

def validate_mapping(config):
    entries = config[CONF_ENTRIES]
    from_ = config[CONF_FROM]
    to_ = config[CONF_TO]
    if len(entries) == 0:
        raise cv.Invalid("Map must have at least one entry")
    if isinstance(to_, dict):
        class_ = to_[CONF_CLASS]
        to_val = cv.use_id(class_)
    else:
        to_val = INDEX_TYPES[to_].validator
    schema = BASE_SCHEMA.extend({
            cv.Required(CONF_ENTRIES):cv.Schema({ INDEX_TYPES[from_].validator: to_val})
        })
    return schema(config)

CONFIG_SCHEMA = cv.All( BASE_SCHEMA, validate_mapping )

async def to_code(config):
    entries = config[CONF_ENTRIES]
    index = config[CONF_FROM]
    value_spec = config[CONF_TO]
    index_conversion = INDEX_TYPES[index].conversion
    index_type = INDEX_TYPES[index].data_type
    if isinstance(value_spec, dict):
        entries = { index_conversion(key): await cg.get_variable(value) for key, value in entries.items() }
        value_type = MockObj.known_classes[value_spec[CONF_CLASS]].operator("ptr")
    else:
        value_conversion = INDEX_TYPES[value_spec].conversion
        value_type = INDEX_TYPES[value_spec].data_type
        entries = {
            index_conversion(key): value_conversion(value) for key, value in entries.items()
        }
    varid = config[CONF_ID]
    varid.type = map_.template(index_type, value_type)
    var = MockObj(varid, ".")
    decl = VariableDeclarationExpression(varid.type, "", varid)
    add_global(decl)
    CORE.register_variable(varid, var)

    for key, value in entries.items():
        cg.add(var.insert((key, value)))
    return var
