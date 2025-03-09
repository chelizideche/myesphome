import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@P4uLT"]
CONF_ES8388_ID = "es8388_id"

DEPENDENCIES = ["i2c"]


es8388_ns = cg.esphome_ns.namespace("es8388")
ES8388Hub = es8388_ns.class_("ES8388Hub", cg.Component)

ES8388_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ES8388_ID): cv.use_id(ES8388Hub),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ES8388Hub),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
