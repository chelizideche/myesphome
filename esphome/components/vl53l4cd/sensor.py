import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    UNIT_MILLIMETER,
    ICON_RULER,
    DEVICE_CLASS_DISTANCE,
)

CODEOWNERS = ["@yourname"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]

vl53l4cd_ns = cg.esphome_ns.namespace("vl53l4cd")
VL53L4CDSensor = vl53l4cd_ns.class_("VL53L4CDSensor", cg.PollingComponent, sensor.Sensor)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_MILLIMETER,
        icon=ICON_RULER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
    )
    .extend({
        cv.GenerateID(): cv.declare_id(VL53L4CDSensor),
    })
    .extend(i2c.i2c_device_schema(0x52))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)