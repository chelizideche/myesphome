from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_LENGTH,
    CONF_MODEL,
    CONF_REVERSED,
)

CODEOWNERS = ["@jdsteffen"]

serial_multiplexed_display_ns = cg.esphome_ns.namespace("serial_multiplexed_display")
SerialMultiplexedDisplay = serial_multiplexed_display_ns.class_(
    "SerialMultiplexedDisplay", cg.PollingComponent
)
SerialMultiplexedDisplayRef = SerialMultiplexedDisplay.operator("ref")

SerialDeviceModel = serial_multiplexed_display_ns.enum("SerialDeviceModel")
SERIAL_DEVICE_MODELS = {
    "74hc595": SerialDeviceModel.SN74HC595,
    "ct1642": SerialDeviceModel.CT1642,
}

CONF_LATCH_PIN = "latch_pin"
CONF_COMMON_CATHODE = "common_cathode"
SERIAL_DEVICE_MODEL = "model"


def _validate(config):
    if config.get(SERIAL_DEVICE_MODEL) == "74hc595" and CONF_LATCH_PIN not in config:
        raise cv.Invalid("Latch pin must be set when using SN74HC595 devices.")

    return config


CONFIG_SCHEMA = cv.All(
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SerialMultiplexedDisplay),
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_MODEL): cv.enum(SERIAL_DEVICE_MODELS),
            cv.Optional(CONF_REVERSED, default=False): cv.boolean,
            cv.Optional(CONF_COMMON_CATHODE, default=False): cv.boolean,
            cv.Optional(CONF_LENGTH, default=4): cv.All(
                cv.uint8_t, cv.Range(min=0, max=4)
            ),
        }
    ).extend(cv.polling_component_schema("1s")),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))
    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))

    if config.get(CONF_LATCH_PIN):
        latch_pin = await cg.gpio_pin_expression(config[CONF_LATCH_PIN])
        cg.add(var.set_latch_pin(latch_pin))

    cg.add(var.set_common_cathode(config[CONF_COMMON_CATHODE]))

    SERIAL_DEVICE_MODEL = config.get(CONF_MODEL)
    cg.add(var.set_model(SERIAL_DEVICE_MODEL))

    cg.add(var.set_length(config[CONF_LENGTH]))

    cg.add(var.set_reversed(config[CONF_REVERSED]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(SerialMultiplexedDisplayRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
