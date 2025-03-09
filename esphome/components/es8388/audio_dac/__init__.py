import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_ES8388_ID, ES8388_BASE_SCHEMA, es8388_ns

ES8388 = es8388_ns.class_("ES8388", AudioDac, cg.Component, i2c.I2CDevice)


CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(ES8388)})
    .extend(ES8388_BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    parent = await cg.get_variable(config[CONF_ES8388_ID])
    cg.add(parent.register_audio_dac(var))
