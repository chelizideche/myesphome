import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SAMPLE_RATE

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_ADS1100_ID = "ads1100_id"
CONF_GAIN = "gain"

ads1100_ns = cg.esphome_ns.namespace("ads1100")
ADS1100Component = ads1100_ns.class_("ADS1100Component", cg.Component, i2c.I2CDevice)

ADS1100_GAIN = ads1100_ns.enum("ADS1100Gain")
GAIN_OPTIONS = {
    "1": ADS1100_GAIN.ADS1100_GAIN_1,
    "2": ADS1100_GAIN.ADS1100_GAIN_2,
    "4": ADS1100_GAIN.ADS1100_GAIN_4,
    "8": ADS1100_GAIN.ADS1100_GAIN_8,
}

ADS1100_SAMPLE_RATE = ads1100_ns.enum("ADS1100SampleRate")
SAMPLE_RATE_OPTIONS = {
    "128": ADS1100_SAMPLE_RATE.ADS1100_SAMPLE_RATE_128_SPS,
    "32": ADS1100_SAMPLE_RATE.ADS1100_SAMPLE_RATE_32_SPS,
    "16": ADS1100_SAMPLE_RATE.ADS1100_SAMPLE_RATE_16_SPS,
    "8": ADS1100_SAMPLE_RATE.ADS1100_SAMPLE_RATE_8_SPS,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1100Component),
            cv.Optional(CONF_GAIN, default="1"): cv.enum(GAIN_OPTIONS, upper=True),
            cv.Optional(CONF_SAMPLE_RATE, default="128"): cv.enum(
                SAMPLE_RATE_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(None))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_GAIN in config:
        cg.add(var.set_gain(config[CONF_GAIN]))
    if CONF_SAMPLE_RATE in config:
        cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
