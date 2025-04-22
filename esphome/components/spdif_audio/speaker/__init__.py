from esphome import pins
import esphome.codegen as cg
from esphome.components import speaker
from esphome.components.i2s_audio import I2SAudioComponent
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_DURATION,
    CONF_DATA_PIN,
    CONF_DEBUG,
    CONF_ID,
    CONF_NEVER,
    CONF_SAMPLE_RATE,
    CONF_TIMEOUT,
)

from .. import spdif_audio_ns

DEPENDENCIES = ["i2s_audio"]
CODEOWNERS = ["@johnboiles"]

SPDIFSpeaker = spdif_audio_ns.class_(
    "SPDIFSpeaker", cg.Component, speaker.Speaker, I2SAudioComponent
)

CONF_I2S_AUDIO_ID = "i2s_audio_id"

CONF_FILL_SILENCE = "fill_silence"

CONFIG_SCHEMA = speaker.SPEAKER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SPDIFSpeaker),
        cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
        cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.positive_int,
        cv.Optional(
            CONF_BUFFER_DURATION, default="500ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TIMEOUT, default="500ms"): cv.Any(
            cv.positive_time_period_milliseconds,
            cv.one_of(CONF_NEVER, lower=True),
        ),
        cv.Optional(CONF_FILL_SILENCE, default=True): cv.boolean,
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_I2S_AUDIO_ID])
    await speaker.register_speaker(var, config)

    cg.add(var.set_data_pin(config[CONF_DATA_PIN]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
    if config[CONF_TIMEOUT] != CONF_NEVER:
        cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add_define("SPDIF_FILL_SILENCE", config[CONF_FILL_SILENCE])
    cg.add_define("SPDIF_DEBUG", config[CONF_DEBUG])
