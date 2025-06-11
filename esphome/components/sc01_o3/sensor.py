import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_OZONE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@Tthecreator"]

SC01O3_ns = cg.esphome_ns.namespace("sc01_o3")
SC01O3Component = SC01O3_ns.class_(
    "SC01O3Sensor", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_PARTS_PER_BILLION,
        icon=ICON_MOLECULE_CO2,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_OZONE,
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SC01O3Component),
            cv.Optional(
                CONF_UPDATE_INTERVAL, "1000ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sc01_o3",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    # var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
