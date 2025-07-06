import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@duckle29"]
AUTO_LOAD = ["climate_ir"]

climate_ir_panasonic_ns = cg.esphome_ns.namespace("climate_ir_panasonic")
panasonicIrClimate = climate_ir_panasonic_ns.class_(
    "panasonicIrClimate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(panasonicIrClimate),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)
