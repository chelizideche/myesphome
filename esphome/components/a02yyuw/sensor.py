import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODEL,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIMETER,
)

CODEOWNERS = ["@TH-Braemer", "@alexandrezia"]
DEPENDENCIES = ["uart"]

a02yyuw_ns = cg.esphome_ns.namespace("a02yyuw")
A02yyuwComponent = a02yyuw_ns.class_(
    "A02yyuwComponent", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)
Model = a02yyuw_ns.enum("Model")
MODEL = {
    "a02yyuw": Model.A02YYUW,
    "a02yytw": Model.A02YYTW,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        A02yyuwComponent,
        unit_of_measurement=UNIT_MILLIMETER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_DISTANCE,
    )
    .extend(cv.polling_component_schema("100ms"))
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_MODEL, default="a02yyuw"): cv.enum(MODEL, upper=False),
        }
    )
)


def final_validation(config):
    require_tx = True
    if config[CONF_MODEL] == "a02yyuw":
        config.pop(CONF_UPDATE_INTERVAL, None)
        require_tx = False
    schema = uart.final_validate_device_schema(
        "a02yyuw",
        baud_rate=9600,
        require_tx=require_tx,
        require_rx=True,
        data_bits=8,
        parity=None,
        stop_bits=1,
    )
    schema(config)


FINAL_VALIDATE_SCHEMA = final_validation


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
