import pytest

from esphome.components.esp32.gpio_esp32 import (
    _BOARD_GPIO_PIN_VALIDATION_OVERRIDES,
    esp32_validate_gpio_pin,
)
from esphome.const import CONF_BOARD, PLATFORM_ESP32
from esphome.core import CORE


@pytest.fixture(name="mock_esp32_core")
def fixture_mock_esp32_core():
    CORE.data[PLATFORM_ESP32] = {}
    yield
    CORE.data.clear()


def test_gpio_validation_with_board_overrides(mock_esp32_core):
    # Test valid pin configurations for specific boards
    for board, allowed_pins in _BOARD_GPIO_PIN_VALIDATION_OVERRIDES.items():
        CORE.data[PLATFORM_ESP32][CONF_BOARD] = board

        # Test that allowed pins don't raise exceptions
        for pin in allowed_pins:
            assert esp32_validate_gpio_pin(pin) == pin

    # Test invalid board
    CORE.data[PLATFORM_ESP32][CONF_BOARD] = "unknown_board"
    with pytest.raises(Exception) as exc:
        esp32_validate_gpio_pin(7)
    assert "flash interface" in str(exc.value)


def test_gpio_validation_without_board():
    # Test that SDIO pins are rejected when no board is specified
    with pytest.raises(Exception) as exc:
        esp32_validate_gpio_pin(7)
    assert "flash interface" in str(exc.value)


def test_gpio_validation_invalid_pins(mock_esp32_core):
    CORE.data[PLATFORM_ESP32][CONF_BOARD] = "adafruit_feather_esp32_v2"

    # Test invalid pin numbers
    with pytest.raises(Exception) as exc:
        esp32_validate_gpio_pin(-1)
    assert "Invalid pin number" in str(exc.value)

    with pytest.raises(Exception) as exc:
        esp32_validate_gpio_pin(40)
    assert "Invalid pin number" in str(exc.value)
