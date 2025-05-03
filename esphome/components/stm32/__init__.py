import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_STM32,
)
from esphome.core import CORE, coroutine_with_priority

from .boards import detect_board_series
from .const import CONF_BOARD_SERIES, KEY_BOARD, KEY_STM32
from .gpio import stm32_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@mrk"]
# AUTO_LOAD = ["preferences"]
AUTO_LOAD = []
IS_TARGET_PLATFORM = True


def set_core_data(config):
    CORE.data[KEY_STM32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_STM32
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "stm32cube"
    CORE.data[KEY_STM32][KEY_BOARD] = config[CONF_BOARD]
    return config


def get_download_types(storage_json):
    return [
        {
            "title": "UF2 factory format",
            "description": "For copying to RP2040 over USB.",
            "file": "firmware.uf2",
            "download": f"{storage_json.name}.factory.uf2",
        },
        {
            "title": "OTA format",
            "description": "For OTA updating a device.",
            "file": "firmware.ota.bin",
            "download": f"{storage_json.name}.ota.bin",
        },
    ]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict,
            ),
            cv.Optional(CONF_BOARD_SERIES): cv.string_strict,
        }
    ),
    detect_board_series,
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_STM32")
    cg.add_build_flag("-D" + config[CONF_BOARD_SERIES])
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32")

    cg.add_platformio_option("framework", "stm32cube")
    cg.add_platformio_option("platform", "ststm32")
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")
