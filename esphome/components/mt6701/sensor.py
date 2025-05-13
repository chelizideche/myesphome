from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, UNIT_DEGREES

DEPENDENCIES = ["i2c"]

mt6701_ns = cg.esphome_ns.namespace("mt6701")
MT6701Sensor = mt6701_ns.class_("MT6701Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = sensor.sensor_schema(
    MT6701Sensor,
    unit_of_measurement=UNIT_DEGREES,
    icon='mdi:rotate-360',
    accuracy_decimals=3,
).extend(
    i2c.i2c_device_schema(0x06)
).extend(
    cv.polling_component_schema("30s")
)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

