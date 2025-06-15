import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL,CONF_USE_FAHRENHEIT

AUTO_LOAD = ["climate_ir"]

friedrich_ns = cg.esphome_ns.namespace("friedrich")
FriedrichClimate = friedrich_ns.class_(
    "FriedrichClimate", climate_ir.ClimateIR
)

Model = friedrich_ns.enum("Model")
MODELS = {
    "M12YH": Model.MODEL_M12YH,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FriedrichClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="M12YH"): cv.enum(MODELS, upper=True),
        cv.Optional(CONF_USE_FAHRENHEIT, default=True): cv.boolean,
    }
)

async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))
