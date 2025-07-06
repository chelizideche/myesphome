import logging

import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    PLATFORM_ESP32,
)
from esphome.core import CORE
from esphome.pins import check_strapping_pin

_ESP_SDIO_PINS = {
    6: "Flash Clock",
    7: "Flash Data 0",
    8: "Flash Data 1",
    11: "Flash Command",
}

_BOARD_GPIO_PIN_VALIDATION_OVERRIDES = {
    "adafruit_feather_esp32_v2": [7, 8],
    "adafruit_qtpy_esp32": [7, 8],
    "ESP32-Pico-V3-02": [7, 8],
}

_ESP32_STRAPPING_PINS = {0, 2, 5, 12, 15}
_LOGGER = logging.getLogger(__name__)


def _get_board_value():
    if PLATFORM_ESP32 not in CORE.data:
        return None

    return CORE.data[PLATFORM_ESP32].get(CONF_BOARD)


def esp32_validate_gpio_pin(value):
    board_value = _get_board_value()

    if value < 0 or value > 39:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-39)")
    if value in _ESP_SDIO_PINS and not (
        board_value is not None
        and board_value in _BOARD_GPIO_PIN_VALIDATION_OVERRIDES
        and value in _BOARD_GPIO_PIN_VALIDATION_OVERRIDES[board_value]
    ):
        raise cv.Invalid(
            f"This pin cannot be used on ESP32s and is already used by the flash interface (function: {_ESP_SDIO_PINS[value]})"
        )
    if 9 <= value <= 10:
        _LOGGER.warning(
            "Pin %s (9-10) might already be used by the flash interface in QUAD IO flash mode.",
            value,
        )
    if value in (24, 28, 29, 30, 31):
        # These pins are not exposed in GPIO mux (reason unknown)
        # but they're missing from IO_MUX list in datasheet
        raise cv.Invalid(f"The pin GPIO{value} is not usable on ESP32s.")
    return value


def esp32_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    if is_input:
        # All ESP32 pins support input mode
        pass
    if is_output and 34 <= num <= 39:
        raise cv.Invalid(
            f"GPIO{num} (34-39) does not support output pin mode.",
            [CONF_MODE, CONF_OUTPUT],
        )
    if is_pullup and 34 <= num <= 39:
        raise cv.Invalid(
            f"GPIO{num} (34-39) does not support pullups.", [CONF_MODE, CONF_PULLUP]
        )
    if is_pulldown and 34 <= num <= 39:
        raise cv.Invalid(
            f"GPIO{num} (34-39) does not support pulldowns.", [CONF_MODE, CONF_PULLDOWN]
        )

    check_strapping_pin(value, _ESP32_STRAPPING_PINS, _LOGGER)
    return value
