import logging

import esphome.codegen as cg
from esphome.components import esp32, microphone
from esphome.components.adc import (
    ATTENUATION_MODES,
    ESP32_VARIANT_ADC2_PIN_TO_CHANNEL,
    validate_adc_pin,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_ID,
    CONF_NUMBER,
    CONF_SAMPLE_RATE,
    CONF_WIFI,
)
import esphome.final_validate as fv

CODEOWNERS = ["@calumapplepie"]

DEPENDENCIES = ["esp32"]

_LOGGER = logging.getLogger(__name__)

_attenuation = cv.enum(ATTENUATION_MODES, lower=True)

CONF_ADC_PIN = "adc_pin"
CONF_ADC_TYPE = "adc_type"
adc_microphone_ns = cg.esphome_ns.namespace("adc_microphone")

ADCAudioMicrophone = adc_microphone_ns.class_(
    "ADCAudioMicrophone", microphone.Microphone, cg.Component
)

BASE_SCHEMA = microphone.MICROPHONE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ADCAudioMicrophone),
        cv.Optional(CONF_SAMPLE_RATE, default=20000): cv.int_range(min=1),
        cv.Required(CONF_ADC_PIN): validate_adc_pin,
        cv.Optional(CONF_ATTENUATION, default=False): _attenuation,
    }
).extend(cv.COMPONENT_SCHEMA)


def validate_config(config):
    if config.get(CONF_ATTENUATION, None) == "auto":
        raise cv.Invalid("Automatic attenuation not supported for microphone")
    if config.get(CONF_ATTENUATION) == "11db":
        _LOGGER.warning(
            "`attenuation: 11db` is deprecated, use `attenuation: 12db` instead"
        )
        # Alter value here so `config` command prints the recommended change
        config[CONF_ATTENUATION] = _attenuation("12db")

    variant = esp32.get_esp32_variant()
    if variant in [VARIANT_ESP32, VARIANT_ESP32C3, VARIANT_ESP32S3]:
        pin_num = config[CONF_ADC_PIN][CONF_NUMBER]
        if pin_num in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL[variant]:
            raise cv.Invalid("Only ADC1 supported for DMA with this variant")


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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    pin_num = config[CONF_ADC_PIN][CONF_NUMBER]
    cg.add(var.set_adc_channel(pin_num))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))

    attenuation = config.get(CONF_ATTENUATION)
    cg.add(var.set_attenuation(attenuation))
