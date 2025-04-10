"""WTS01 temperature sensor component for Sonoff TH Elite (THR316, THR320, THR316D, THR320D)."""

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UART_ID, CONF_UPDATE_INTERVAL

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@alepee"]

wts01_ns = cg.esphome_ns.namespace("wts01")
WTS01Sensor = wts01_ns.class_("WTS01Sensor", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WTS01Sensor),
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_UPDATE_INTERVAL, default="15s"): cv.update_interval,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_UART_ID in config:
        uart_component = await cg.get_variable(config[CONF_UART_ID])
    else:
        uart_component = await cg.get_variable(config[uart.CONF_UART_ID])

    cg.add(var.construct(uart_component))
