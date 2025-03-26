import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_SAMPLE_RATE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

from .. import (
    CONF_ADS1100_ID,
    GAIN_OPTIONS,
    SAMPLE_RATE_OPTIONS,
    ADS1100Component,
    ads1100_ns,
)

AUTO_LOAD = ["ads1100"]
DEPENDENCIES = ["ads1100"]

ADS1100Sensor = ads1100_ns.class_("ADS1100Sensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ADS1100Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS1100_ID): cv.use_id(ADS1100Component),
            cv.Optional(CONF_GAIN, default="1"): cv.enum(GAIN_OPTIONS, upper=True),
            cv.Optional(CONF_SAMPLE_RATE, default="128"): cv.enum(
                SAMPLE_RATE_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS1100_ID])

    if CONF_GAIN in config:
        cg.add(var.set_gain(config[CONF_GAIN]))
    if CONF_SAMPLE_RATE in config:
        cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
