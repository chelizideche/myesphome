from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]

emerio_pac_125152_ns = cg.esphome_ns.namespace("emerio_pac_125152")
EmerioPac125152Climate = emerio_pac_125152_ns.class_(
    "EmerioPac125152Climate", climate_ir.ClimateIR
)
CalibrateStateAction = emerio_pac_125152_ns.class_(
    "CalibrateStateAction", automation.Action
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(EmerioPac125152Climate)

CALIBRATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(EmerioPac125152Climate),
        cv.Required("mode"): cv.templatable(cv.int_),
        cv.Required("temperature"): cv.templatable(cv.float_),
        cv.Required("fan_mode"): cv.templatable(cv.int_),
    }
)


@automation.register_action(
    "emerio_pac_125152.calibrate_state",
    CalibrateStateAction,
    CALIBRATE_ACTION_SCHEMA,
)
async def emerio_pac_125152_calibrate_state_action_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    templatable_mode = await cg.templatable(config["mode"], args, int)
    cg.add(var.set_mode(templatable_mode))
    templatable_temp = await cg.templatable(config["temperature"], args, float)
    cg.add(var.set_temperature(templatable_temp))
    templatable_fan = await cg.templatable(config["fan_mode"], args, int)
    cg.add(var.set_fan_mode(templatable_fan))
    return var


async def to_code(config):
    await climate_ir.new_climate_ir(config)
