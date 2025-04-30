import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import ICON_RULER, UNIT_CENTIMETER

DEPENDENCIES = ["uart"]

ld2413_ns = cg.esphome_ns.namespace("ld2413")
LD2413Sensor = ld2413_ns.class_(
    "LD2413Sensor", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_CENTIMETER,
        icon=ICON_RULER,
        accuracy_decimals=2,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(LD2413Sensor),
        }
    )
    .extend(cv.polling_component_schema("0.5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
