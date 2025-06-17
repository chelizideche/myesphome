import esphome.codegen as cg
from esphome.components import one_wire
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@autoantwort"]

DEPENDENCIES = ["one_wire"]
MULTI_CONF = True
CONF_DS2406_ID = "ds2406_id"


ds2406_ns = cg.esphome_ns.namespace("ds2406")

Ds2406 = ds2406_ns.class_(
    "Ds2406",
    cg.PollingComponent,
    one_wire.OneWireDevice,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ds2406),
        }
    )
    .extend(one_wire.one_wire_device_schema())
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await one_wire.register_one_wire_device(var, config)
