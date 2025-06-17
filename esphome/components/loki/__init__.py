import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@jzucker2"]
DEPENDENCIES = ["network"]

loki_component_ns = cg.esphome_ns.namespace("loki")
LokiComponent = loki_component_ns.class_("LokiComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LokiComponent),
        cv.Optional("loki_host"): cv.url,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
