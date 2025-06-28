import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, UNIT_DEGREES

DEPENDENCIES = ["i2c"]

mt6701_ns = cg.esphome_ns.namespace("mt6701")
MT6701Sensor = mt6701_ns.class_(
    "MT6701Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

ICON_ANGLE = "mdi:angle-acute"


def check_keys(obj):
    if obj[CONF_ADDRESS] not in [0x06, 0x26]:
        msg = "MT6701 Address can only be 0x06 or 0x26"
        raise cv.Invalid(msg)
    return obj


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        MT6701Sensor,
        unit_of_measurement=UNIT_DEGREES,
        icon=ICON_ANGLE,
        accuracy_decimals=3,
    )
    .extend(i2c.i2c_device_schema(0x06))
    .extend(cv.polling_component_schema("30s")),
    check_keys,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
