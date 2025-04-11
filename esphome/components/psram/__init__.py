import logging

import esphome.codegen as cg
from esphome.components.esp32 import (
    CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES,
    VARIANT_ESP32,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_MODE,
    CONF_SPEED,
    PLATFORM_ESP32,
)
from esphome.core import CORE
import esphome.final_validate as fv

CODEOWNERS = ["@esphome/core"]

DEPENDENCIES = [PLATFORM_ESP32]

_LOGGER = logging.getLogger(__name__)

psram_ns = cg.esphome_ns.namespace("psram")
PsramComponent = psram_ns.class_("PsramComponent", cg.Component)

TYPE_QUAD = "quad"
TYPE_OCTAL = "octal"
TYPE_HEX = "hex"

CONF_ENABLE_ECC = "enable_ecc"

SPIRAM_MODES = {
    VARIANT_ESP32: [TYPE_QUAD],
    VARIANT_ESP32S2: [TYPE_QUAD],
    VARIANT_ESP32S3: [TYPE_QUAD, TYPE_OCTAL],
    VARIANT_ESP32P4: [TYPE_HEX],
}


SPIRAM_SPEEDS = {
    VARIANT_ESP32: (40, 80, 120),
    VARIANT_ESP32S2: (40, 80, 120),
    VARIANT_ESP32S3: (40, 80, 120),
    VARIANT_ESP32P4: (20, 100, 200),
}


def validate_psram_mode(config):
    if config[CONF_MODE] == TYPE_OCTAL and config[CONF_SPEED] == 120e6:
        esp32_config = fv.full_config.get()[PLATFORM_ESP32]
        if (
            esp32_config[CONF_FRAMEWORK]
            .get(CONF_ADVANCED, {})
            .get(CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES)
        ):
            _LOGGER.warning(
                "120MHz PSRAM in octal mode is an experimental feature - use at your own risk"
            )
        else:
            raise cv.Invalid("PSRAM 120MHz is not supported in octal mode")
    if config[CONF_MODE] == TYPE_QUAD and config[CONF_ENABLE_ECC]:
        raise cv.Invalid("ECC is not available in quad mode.")
    return config


def get_config_schema(config):
    variant = get_esp32_variant()
    speeds = [f"{s}MHZ" for s in SPIRAM_SPEEDS.get(variant, [])]
    if not speeds:
        return cv.Invalid("PSRAM is not supported on this chip")
    modes = SPIRAM_MODES[variant]
    return cv.All(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(PsramComponent),
                cv.Optional(CONF_MODE, default=modes[0]): cv.one_of(*modes, lower=True),
                cv.Optional(CONF_ENABLE_ECC, default=False): cv.boolean,
                cv.Optional(CONF_SPEED, default=speeds[0]): cv.one_of(
                    *speeds, upper=True
                ),
            }
        ),
        validate_psram_mode,
    )(config)


CONFIG_SCHEMA = get_config_schema

FINAL_VALIDATE_SCHEMA = validate_psram_mode


async def to_code(config):
    if CORE.using_arduino:
        cg.add_build_flag("-DBOARD_HAS_PSRAM")
        if config[CONF_MODE] == TYPE_OCTAL:
            cg.add_platformio_option("board_build.arduino.memory_type", "qio_opi")

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option(
            f"CONFIG_{get_esp32_variant().upper()}_SPIRAM_SUPPORT", True
        )
        add_idf_sdkconfig_option("CONFIG_SOC_SPIRAM_SUPPORTED", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_CAPS_ALLOC", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_IGNORE_NOTFOUND", True)

        mode = config[CONF_MODE]
        if mode == TYPE_OCTAL:
            mode = "oct"
        add_idf_sdkconfig_option(f"CONFIG_SPIRAM_MODE_{mode.upper()}", True)

        speed = int(config[CONF_SPEED][:-3])
        add_idf_sdkconfig_option(f"CONFIG_SPIRAM_SPEED_{speed}M", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_SPEED", speed)
        if config[CONF_MODE] == TYPE_OCTAL and config[CONF_SPEED] == 120e6:
            add_idf_sdkconfig_option("CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240", True)
            # This works only on IDF 5.4.x but does no harm on earlier versions
            add_idf_sdkconfig_option(
                "CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR", True
            )
        if config[CONF_ENABLE_ECC]:
            add_idf_sdkconfig_option("CONFIG_SPIRAM_ECC_ENABLE", True)

    cg.add_define("USE_PSRAM")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
