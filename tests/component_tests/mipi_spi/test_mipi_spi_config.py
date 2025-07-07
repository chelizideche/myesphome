"""Tests for mpip_spi configuration validation."""

from pathlib import Path

import pytest

from esphome import config_validation as cv, final_validate
from esphome.components.esp32 import (
    FRAMEWORK_ARDUINO,
    FRAMEWORK_ESP_IDF,
    KEY_BOARD,
    KEY_ESP32,
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32P4,
    VARIANT_ESP32S3,
)
from esphome.components.esp32.gpio import validate_gpio_pin
from esphome.config import Config
from esphome.const import (
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_WIDTH,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
    PLATFORM_ESP32,
)
from esphome.core import CORE
from esphome.pins import gpio_output_pin_schema

_VARIANTS = (VARIANT_ESP32S3, VARIANT_ESP32C3, VARIANT_ESP32P4, VARIANT_ESP32)

_mock_config = Config()


def _set_component_config(name, value):
    """Set a component configuration in the mock config."""
    _mock_config[name] = value


def _config_setup():
    from esphome import config
    from esphome.core import CORE

    CORE.config_path = __file__
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_ESP32,
        KEY_TARGET_FRAMEWORK: FRAMEWORK_ESP_IDF,
    }
    CORE.data[KEY_ESP32] = {
        KEY_VARIANT: VARIANT_ESP32S3,
        KEY_BOARD: "esp32-s3-devkitc-1",
    }
    config.path_context.set([])
    final_validate.full_config.set(_mock_config)


def _test_failure(config, error_msg: str, validator=None) -> None:
    """Helper function to test failure of configuration validation."""
    from esphome.components.mipi_spi.display import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    with pytest.raises(cv.Invalid) as exc_info:
        if validator:
            validator(config)
        else:
            FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))
    assert error_msg in str(exc_info.value)


def test_configuration_errors() -> None:
    """Test detection of invalid configuration"""

    _config_setup()

    from esphome.components.mipi_spi.display import dimension_schema
    from esphome.core import CORE

    _test_failure("a string", "expected a dictionary")
    _test_failure({"id": "display_id"}, "required key not provided @ data['model']")
    _test_failure(
        {"id": "display_id", "model": "custom", "init_sequence": [[0x36, 0x01]]},
        "required key not provided @ data['dimensions']",
    )
    _test_failure(
        {"model": "custom", "dc_pin": 11, "dimensions": {"width": 320, "height": 240}},
        "required key not provided @ data['init_sequence']",
    )

    _test_failure(
        {
            "id": "display_id",
            "model": "custom",
            "dimensions": {"width": 320, "height": 240},
            "draw_rounding": 13,
        },
        "value must be a power of two for dictionary value @ data['draw_rounding']",
    )
    _test_failure(
        {"width": 320},
        "required key not provided @ data['height']",
        dimension_schema(4),
    )
    _test_failure(
        {"width": 320, "height": 111},
        "Dimensions and offsets must be divisible by 32",
        dimension_schema(32),
    )

    _test_failure(
        {
            "model": "JC3248W535",
            "transform": {"mirror_x": False, "mirror_y": True, "swap_xy": True},
        },
        "Axis swapping not supported by this model",
    )
    _test_failure(
        {
            "model": "JC3248W535",
            "transform": {"mirror_x": False, "mirror_y": True, "swap_xy": False},
            "init_sequence": [[0x36, 0x01]],
        },
        "transform is not supported when MADCTL (0X36) is in the init sequence",
    )
    _test_failure(
        {"model": "JC3248W535", "init_sequence": [[0x3A, 0x01]]},
        "PIXFMT (0X3A) should not be in the init sequence, it will be set automatically",
    )
    _test_failure(
        {"model": "t4-s3", "dc_pin": 11},
        "DC pin is not supported in quad mode",
    )

    _test_failure(
        {"model": "t4-s3", "color_depth": 18},
        "Unknown value '18', valid options are '16', '16bit",
    )

    _test_failure(
        {"model": "t-embed", "color_depth": 24},
        "Unknown value '24', valid options are '16', '8",
    )

    _test_failure(
        {"model": "ili9488"},
        "DC pin is required in single mode",
    )
    # Brightness is not supported except for specific models
    _test_failure(
        {"model": "wt32-sc01-plus", "brightness": 128},
        "extra keys not allowed @ data['brightness']",
    )
    _test_failure(
        {
            "model": "T-DISPLAY-S3-PRO",
        },
        "PSRAM is required for this display",
    )

    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = FRAMEWORK_ARDUINO
    _test_failure(
        {"model": "wt32-sc01-plus"},
        "This feature is only available with frameworks ['esp-idf']",
    )

    CORE.reset()


def _set_variant(model):
    """
    Get the ESP32 variant for the given model based on pins
    """
    from esphome.core import CORE

    for v in _VARIANTS:
        try:
            CORE.data[KEY_ESP32][KEY_VARIANT] = v
            for pin in [
                model.get_default(pin, None)
                for pin in ("dc_pin", "reset_pin", "cs_pin")
            ]:
                if pin is not None:
                    pin = gpio_output_pin_schema(pin)
                    validate_gpio_pin(pin)
            return
        except cv.Invalid:
            continue


def _success(config):
    """Helper function to test successful configuration validation."""
    from esphome.components.mipi_spi.display import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))


def test_configuration_success() -> None:
    """Test successful configuration validation."""
    _config_setup()

    from esphome.components.mipi_spi.display import (
        CONF_BUS_MODE,
        CONF_NATIVE_HEIGHT,
        MODELS,
    )

    # Custom model with all options
    _success(
        {
            "model": "custom",
            "pixel_mode": "18bit",
            "color_depth": 8,
            "id": "display_id",
            "byte_order": "little_endian",
            "bus_mode": "single",
            "color_order": "rgb",
            "dc_pin": 11,
            "reset_pin": 12,
            "enable_pin": 13,
            "cs_pin": 14,
            "init_sequence": [[0xA0, 0x01]],
            "dimensions": {
                "width": 320,
                "height": 240,
                "offset_width": 32,
                "offset_height": 32,
            },
            "invert_colors": True,
            "transform": {"mirror_x": True, "mirror_y": True, "swap_xy": False},
            "spi_mode": "mode0",
            "data_rate": "40MHz",
            "use_axis_flips": True,
            "draw_rounding": 4,
            "spi_16": True,
            "buffer_size": 0.25,
        }
    )

    # Enable PSRAM for the remainder
    _set_component_config("psram", True)

    # Test all models, providing default values where necessary
    for name, model in MODELS.items():
        config = {"model": name}
        _set_variant(model)
        if (
            not model.get_default(CONF_DC_PIN)
            and model.get_default(CONF_BUS_MODE) != "quad"
        ):
            config[CONF_DC_PIN] = 14
        if not model.get_default(CONF_NATIVE_HEIGHT):
            config[CONF_DIMENSIONS] = {CONF_HEIGHT: 240, CONF_WIDTH: 320}
        if model.initsequence is None:
            config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]
        _success(config)

    CORE.reset()


def _get_path(file_name: str) -> Path:
    """Helper function to get the absolute path of a file relative to the test script."""
    return (Path(__file__).parent / file_name).absolute()


def test_native_generation(generate_main) -> None:
    """Test code generation for display."""

    main_cpp = generate_main(_get_path("native.yaml"))
    assert (
        "mipi_spi::MipiSpiBuffer<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_QUAD, 360, 360, 0, 1, display::DISPLAY_ROTATION_0_DEGREES, 1>()"
        in main_cpp
    )
    assert "set_init_sequence({240, 1, 8, 242" in main_cpp
    assert "show_test_card();" in main_cpp
    assert "set_write_only(true);" in main_cpp

    CORE.reset()

    main_cpp = generate_main(_get_path("lvgl.yaml"))
    assert (
        "mipi_spi::MipiSpi<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_SINGLE, 128, 160, 0, 0>();"
        in main_cpp
    )
    assert "set_init_sequence({1, 0, 10, 255, 177" in main_cpp
    assert "show_test_card();" not in main_cpp
    assert "set_auto_clear(false);" in main_cpp
