import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_DURATION,
    CONF_ID,
    CONF_MULTIPLY,
    CONF_NUM_CHANNELS,
    CONF_OFFSET,
    CONF_OUTPUT_SPEAKER,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)
from esphome.core.entity_helpers import inherit_property_from

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@calumapplepie"]

CONF_UNSIGNED = "convert_to_unsigned"
CONF_INTERCEPT_VOLUME = "intercept_volume"

speaker_math_ns = cg.esphome_ns.namespace("speaker_math")
SpeakerMath = speaker_math_ns.class_("SpeakerMath", cg.Component, speaker.Speaker)


def _set_stream_limits(config):
    audio.set_stream_limits(
        min_bits_per_sample=8,
        max_bits_per_sample=32,
    )(config)

    return config


def _validate_audio_compatability(config):
    inherit_property_from(CONF_BITS_PER_SAMPLE, CONF_OUTPUT_SPEAKER)(config)
    inherit_property_from(CONF_NUM_CHANNELS, CONF_OUTPUT_SPEAKER)(config)
    inherit_property_from(CONF_SAMPLE_RATE, CONF_OUTPUT_SPEAKER)(config)

    audio.final_validate_audio_schema(
        "source_speaker",
        audio_device=CONF_OUTPUT_SPEAKER,
        bits_per_sample=config.get(CONF_BITS_PER_SAMPLE),
        channels=config.get(CONF_NUM_CHANNELS),
        sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)

    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SpeakerMath),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(
                CONF_BUFFER_DURATION, default="10ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_UNSIGNED, default=False): cv.boolean,
            cv.Optional(CONF_MULTIPLY, default=1): cv.int_range(-(2**7), 2**7 - 1),
            cv.Optional(CONF_OFFSET, default=0): cv.int_range(-(2**15), 2**15 - 1),
            cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_INTERCEPT_VOLUME, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
    _set_stream_limits,
)


def _validate_audio_intercept(config):
    if config[CONF_UNSIGNED]:
        if config[CONF_INTERCEPT_VOLUME]:
            raise cv.Invalid(
                "Must intercept volume control if you want to use unsigned data"
            )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(_validate_audio_compatability, _validate_audio_intercept)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)

    output_spkr = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])
    cg.add(var.set_output_speaker(output_spkr))

    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))

    cg.add(var.set_target_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))

    cg.add(var.set_convert_unsigned(config[CONF_UNSIGNED]))
    cg.add(var.set_convert_factor(config[CONF_MULTIPLY]))
    cg.add(var.set_convert_offset(config[CONF_OFFSET]))

    if task_stack_in_psram := config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(task_stack_in_psram))
        if task_stack_in_psram:
            if config[CONF_TASK_STACK_IN_PSRAM]:
                esp32.add_idf_sdkconfig_option(
                    "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
                )
