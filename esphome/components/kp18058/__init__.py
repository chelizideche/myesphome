from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_ID

AUTO_LOAD = ["i2c_soft"]
CODEOWNERS = ["@NewoPL"]

MULTI_CONF = True

CONF_CW_CURRENT = "cw_current"
CONF_RGB_CURRENT = "rgb_current"

KP18058_ns = cg.esphome_ns.namespace("kp18058")
KP18058 = KP18058_ns.class_("KP18058", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(KP18058),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_CW_CURRENT, default=8): cv.float_range(min=0, max=77.5),
        cv.Optional(CONF_RGB_CURRENT, default=5): cv.float_range(min=0, max=48),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_cw_current(config[CONF_CW_CURRENT]))
    cg.add(var.set_rgb_current(config[CONF_RGB_CURRENT]))

    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    clock = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_i2c_pins(data, clock))
