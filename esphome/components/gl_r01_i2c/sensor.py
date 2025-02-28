import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIMETER,
)

CODEOWNERS = ["@pkejval"]
DEPENDENCIES = ["i2c"]

CONF_MIN_READ_INTERVAL = "min_read_interval"

gl_r01_i2c_ns = cg.esphome_ns.namespace("gl_r01_i2c")
GLR01I2CComponent = gl_r01_i2c_ns.class_(
    "GLR01I2CComponent", i2c.I2CDevice, cg.PollingComponent
)


# Ensure min_read_interval is at least 40ms as stated in datasheet
def validate_min_read_interval(value):
    value = cv.positive_time_period_milliseconds(value)
    if value.total_milliseconds < 40:
        raise cv.Invalid(f"min_read_interval must be at least 40ms (got {value})")
    return value


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        GLR01I2CComponent,
        unit_of_measurement=UNIT_MILLIMETER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x74))
    .extend(
        {
            cv.Optional(
                CONF_MIN_READ_INTERVAL, default="40ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)

    # Set the min_read_interval if provided in config
    if CONF_MIN_READ_INTERVAL in config:
        cg.add(var.set_min_read_interval(config[CONF_MIN_READ_INTERVAL]))
