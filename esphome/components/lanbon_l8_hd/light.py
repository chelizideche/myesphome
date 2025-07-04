import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, light, output
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_ENABLE_PIN,
)

DEPENDENCIES = ["uart", "light"]

lanbon_l8_hd_ns = cg.esphome_ns.namespace("lanbon_l8_hd")
LanbonL8DSOutput = lanbon_l8_hd_ns.class_(
    "LocalDimmerOutput", cg.Component, uart.UARTDevice, light.LightOutput
)

CONFIG_SCHEMA = (
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LanbonL8DSOutput),
            cv.Optional(CONF_MIN_VALUE, default=0): cv.int_range(min=0, max=100),
            cv.Optional(CONF_MAX_VALUE, default=100): cv.int_range(min=0, max=100),
            cv.Required(CONF_ENABLE_PIN): cv.use_id(output.BinaryOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "lanbon_l8_hd",
    baud_rate=115200,
    data_bits=8,
    parity=None,
    stop_bits=1,
    require_tx=True,
    require_rx=False,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    await light.register_light(var, config)
    enable = await cg.get_variable(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable(enable))
