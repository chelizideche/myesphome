import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@jemeckle"]

DEPENDENCIES = ["i2c"]

s300_ns = cg.esphome_ns.namespace("s300")
S300Component = s300_ns.class_("S300Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        S300Component,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        device_class=DEVICE_CLASS_CARBON_DIOXIDE,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x31))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
