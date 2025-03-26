import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["i2c"]

ads1100_ns = cg.esphome_ns.namespace("ads1100")
ADS1100Component = ads1100_ns.class_("ADS1100Component", cg.Component, i2c.I2CDevice)
ADS1100Sensor = ads1100_ns.class_("ADS1100Sensor", sensor.Sensor, cg.PollingComponent)

ADS1100_GAIN = ads1100_ns.enum("ADS1100Gain")
GAIN_OPTIONS = {
    "1": ADS1100_GAIN.ADS1100_GAIN_1,
    "2": ADS1100_GAIN.ADS1100_GAIN_2,
    "4": ADS1100_GAIN.ADS1100_GAIN_4,
    "8": ADS1100_GAIN.ADS1100_GAIN_8,
}

ADS1100_DATA_RATE = ads1100_ns.enum("ADS1100DataRate")
DATA_RATE_OPTIONS = {
    "128": ADS1100_DATA_RATE.ADS1100_DATA_RATE_128_SPS,
    "32": ADS1100_DATA_RATE.ADS1100_DATA_RATE_32_SPS,
    "16": ADS1100_DATA_RATE.ADS1100_DATA_RATE_16_SPS,
    "8": ADS1100_DATA_RATE.ADS1100_DATA_RATE_8_SPS,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1100Component),
            cv.Optional("gain", default="1"): cv.enum(GAIN_OPTIONS, upper=True),
            cv.Optional("data_rate", default="128"): cv.enum(
                DATA_RATE_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x48))
)

SENSOR_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ADS1100Sensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if "gain" in config:
        cg.add(var.set_gain(config["gain"]))
    if "data_rate" in config:
        cg.add(var.set_data_rate(config["data_rate"]))


async def sensor_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_parent(cg.new_Pvariable(config[CONF_ID])))
