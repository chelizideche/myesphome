import logging

import esphome.codegen as cg
from esphome.components import esp32, microphone
from esphome.components.adc import ESP32_VARIANT_ADC2_PIN_TO_CHANNEL, validate_adc_pin
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_BUFFER_DURATION,
    CONF_ID,
    CONF_NUMBER,
    CONF_SAMPLE_RATE,
    CONF_WIFI,
)
import esphome.final_validate as fv

CODEOWNERS = ["@calumapplepie"]

DEPENDENCIES = ["esp32"]

_LOGGER = logging.getLogger(__name__)

CONF_FREE_MEM = "free_memory_on_stop"

ATTENUATION_MODES = {
    "0db": cg.global_ns.ADC_ATTEN_DB_0,
    "2.5db": cg.global_ns.ADC_ATTEN_DB_2_5,
    "6db": cg.global_ns.ADC_ATTEN_DB_6,
    "12db": cg.global_ns.ADC_ATTEN_DB_12,
}

_attenuation = cv.enum(ATTENUATION_MODES, lower=True)

CONF_ADC_PIN = "adc_pin"
adc_microphone_ns = cg.esphome_ns.namespace("adc_microphone")

ADCAudioMicrophone = adc_microphone_ns.class_(
    "ADCAudioMicrophone", microphone.Microphone, cg.Component
)

BASE_SCHEMA = microphone.MICROPHONE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ADCAudioMicrophone),
        cv.Optional(CONF_SAMPLE_RATE, default=32000): cv.int_range(min=1),
        cv.Optional(
            CONF_BUFFER_DURATION, default="100ms"
        ): cv.positive_time_period_milliseconds,
        cv.Required(CONF_ADC_PIN): validate_adc_pin,
        cv.Optional(CONF_ATTENUATION, default="12db"): _attenuation,
    }
).extend(cv.COMPONENT_SCHEMA)


def validate_config(config):
    variant = esp32.get_esp32_variant()
    if variant in [VARIANT_ESP32, VARIANT_ESP32C3, VARIANT_ESP32S3]:
        pin_num = config[CONF_ADC_PIN][CONF_NUMBER]
        if pin_num in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL[variant]:
            raise cv.Invalid("Only ADC1 supported for DMA with this variant")
    return config


def final_validate_config(config):
    variant = esp32.get_esp32_variant()
    if (
        CONF_WIFI in fv.full_config.get()
        and config[CONF_ADC_PIN][CONF_NUMBER]
        in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL[variant]
    ):
        raise cv.Invalid(
            f"{variant} doesn't support ADC on this pin when Wi-Fi is configured"
        )

    return config


CONFIG_SCHEMA = cv.All(
    BASE_SCHEMA,
    cv.require_framework_version(esp_idf=cv.Version(5, 0, 0)),
    validate_config,
)

FINAL_VALIDATE_SCHEMA = final_validate_config


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    pin_num = config[CONF_ADC_PIN][CONF_NUMBER]
    cg.add(var.set_adc_channel(pin_num))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))

    attenuation = config.get(CONF_ATTENUATION)
    cg.add(var.set_attenuation(attenuation))

    # determine the number of frames based on requested buffer duration
    frames_per_ms = config[CONF_SAMPLE_RATE] / 1000 / 64
    cg.add_define(
        "ADC_DMA_NUM_FRAMES",
        frames_per_ms * config[CONF_BUFFER_DURATION].total_milliseconds,
    )
