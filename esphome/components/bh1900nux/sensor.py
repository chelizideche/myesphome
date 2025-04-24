import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import ICON_EMPTY, UNIT_EMPTY

DEPENDENCIES = ["i2c"]


sensor_ns = cg.esphome_ns.namespace("bh1900nux_i2c")
BH1900NUXSensor = sensor_ns.class_(
    "BH1900NUXSensor", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        BH1900NUXSensor,
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_EMPTY,
        accuracy_decimals=1,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x01))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
