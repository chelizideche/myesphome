from esphome.const import CONF_ID
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, number
from esphome import pins

CONF_BCK_PIN = "bck_pin"
CONF_WS_PIN = "ws_pin"
CONF_DATA_IN_PIN = "data_in_pin"
CONF_CALIBRATION_OFFSET = "calibration_offset"
CONF_CALIBRATION_NUMBER = "calibration_number"

CODEOWNERS = ["@DanBar1971"]

i2s_microfone_ns = cg.esphome_ns.namespace("i2s_microfone")
I2SMicrofoneSensor = i2s_microfone_ns.class_(
    "I2SMicrofoneSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = sensor.sensor_schema().extend({
    cv.GenerateID(): cv.declare_id(I2SMicrofoneSensor),
    cv.Required(CONF_BCK_PIN): pins.internal_gpio_input_pin_schema,
    cv.Required(CONF_WS_PIN): pins.internal_gpio_input_pin_schema,
    cv.Required(CONF_DATA_IN_PIN): pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_CALIBRATION_OFFSET, default=0.0): cv.float_,
    cv.Optional(CONF_CALIBRATION_NUMBER): cv.use_id(number.Number),
}).extend(cv.polling_component_schema("1s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    bck = await cg.gpio_pin_expression(config[CONF_BCK_PIN])
    ws = await cg.gpio_pin_expression(config[CONF_WS_PIN])
    data_in = await cg.gpio_pin_expression(config[CONF_DATA_IN_PIN])
    cg.add(var.set_pins(bck, ws, data_in))

    cg.add(var.set_calibration_offset(config[CONF_CALIBRATION_OFFSET]))

    if CONF_CALIBRATION_NUMBER in config:
        number_var = await cg.get_variable(config[CONF_CALIBRATION_NUMBER])
        cg.add(var.set_calibration_number(number_var))
