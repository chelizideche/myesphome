"""Tests for mpip_spi configuration validation."""

from esphome.components.esp32 import (
    FRAMEWORK_ARDUINO,
    FRAMEWORK_ESP_IDF,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.const import (
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_WIDTH,
    PLATFORM_ESP32,
)


def test_configuration_errors(
    assert_failure, core_reset, set_core_config, choose_variant_with_pins
) -> None:
    """Test detection of invalid configuration"""

    set_core_config(PLATFORM_ESP32, FRAMEWORK_ESP_IDF, "esp32dev", VARIANT_ESP32)

    from esphome.components.mipi_spi.display import dimension_schema

    def test_config(config):
        from esphome.components.mipi_spi.display import (
            CONFIG_SCHEMA,
            FINAL_VALIDATE_SCHEMA,
        )

        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))

    assert_failure(test_config, "expected a dictionary", "a string")
    assert_failure(
        test_config, "required key not provided @ data['model']", {"id": "display_id"}
    )
    assert_failure(
        test_config,
        "required key not provided @ data['dimensions']",
        {"id": "display_id", "model": "custom", "init_sequence": [[0x36, 0x01]]},
    )
    assert_failure(
        test_config,
        "required key not provided @ data['init_sequence']",
        {"model": "custom", "dc_pin": 18, "dimensions": {"width": 320, "height": 240}},
    )

    assert_failure(
        test_config,
        "value must be a power of two for dictionary value @ data['draw_rounding']",
        {
            "id": "display_id",
            "model": "custom",
            "dimensions": {"width": 320, "height": 240},
            "draw_rounding": 13,
        },
    )
    assert_failure(
        dimension_schema(4),
        "required key not provided @ data['height']",
        {"width": 320},
    )
    assert_failure(
        dimension_schema(32),
        "Dimensions and offsets must be divisible by 32",
        {"width": 320, "height": 111},
    )

    assert_failure(
        test_config,
        "Axis swapping not supported by this model",
        {
            "model": "JC3248W535",
            "transform": {"mirror_x": False, "mirror_y": True, "swap_xy": True},
        },
    )
    assert_failure(
        test_config,
        "transform is not supported when MADCTL (0X36) is in the init sequence",
        {
            "model": "custom",
            "dimensions": {"width": 320, "height": 240},
            "transform": {"mirror_x": False, "mirror_y": True, "swap_xy": False},
            "init_sequence": [[0x36, 0x01]],
        },
    )
    assert_failure(
        test_config,
        "PIXFMT (0X3A) should not be in the init sequence, it will be set automatically",
        {
            "model": "custom",
            "dimensions": {"width": 320, "height": 240},
            "init_sequence": [[0x3A, 0x01]],
        },
    )
    set_core_config(PLATFORM_ESP32, FRAMEWORK_ESP_IDF, "esp32dev", VARIANT_ESP32S3)
    assert_failure(
        test_config,
        "DC pin is not supported in quad mode",
        {"model": "t4-s3", "dc_pin": 18},
    )

    assert_failure(
        test_config,
        "Unknown value '18', valid options are '16', '16bit",
        {"model": "t4-s3", "color_depth": 18},
    )

    assert_failure(
        test_config,
        "Unknown value '24', valid options are '16', '8",
        {"model": "t-embed", "color_depth": 24},
    )

    assert_failure(
        test_config, "DC pin is required in single mode", {"model": "ili9488"}
    )
    # Brightness is not supported except for specific models
    assert_failure(
        test_config,
        "extra keys not allowed @ data['brightness']",
        {"model": "wt32-sc01-plus", "brightness": 128},
    )
    assert_failure(
        test_config,
        "PSRAM is required for this display",
        {
            "model": "T-DISPLAY-S3-PRO",
        },
    )

    set_core_config(PLATFORM_ESP32, FRAMEWORK_ARDUINO, "esp32dev", VARIANT_ESP32)

    assert_failure(
        test_config,
        "This feature is only available with frameworks ['esp-idf']",
        {"model": "wt32-sc01-plus"},
    )

    core_reset()


def test_configuration_success(
    set_core_config, set_component_config, choose_variant_with_pins
) -> None:
    """Test successful configuration validation."""
    set_core_config(PLATFORM_ESP32, FRAMEWORK_ESP_IDF, "esp32dev", VARIANT_ESP32S3)

    from esphome.components.mipi_spi.display import (
        CONF_BUS_MODE,
        CONF_NATIVE_HEIGHT,
        MODELS,
    )

    def success(config):
        from esphome.components.mipi_spi.display import (
            CONFIG_SCHEMA,
            FINAL_VALIDATE_SCHEMA,
        )

        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))

    # Custom model with all options
    success(
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
    set_component_config("psram", True)

    # Test all models, providing default values where necessary
    for name, model in MODELS.items():
        config = {"model": name}
        pins = [
            pin
            for pin in [
                model.get_default(pin, None)
                for pin in ("dc_pin", "reset_pin", "cs_pin")
            ]
            if pin is not None
        ]
        choose_variant_with_pins(pins)

        if (
            not model.get_default(CONF_DC_PIN)
            and model.get_default(CONF_BUS_MODE) != "quad"
        ):
            config[CONF_DC_PIN] = 14
        if not model.get_default(CONF_NATIVE_HEIGHT):
            config[CONF_DIMENSIONS] = {CONF_HEIGHT: 240, CONF_WIDTH: 320}
        if model.initsequence is None:
            config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]
        success(config)


def test_native_generation(generate_main, get_path) -> None:
    """Test code generation for display."""

    main_cpp = generate_main(get_path("native.yaml"))
    assert (
        "mipi_spi::MipiSpiBuffer<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_QUAD, 360, 360, 0, 1, display::DISPLAY_ROTATION_0_DEGREES, 1>()"
        in main_cpp
    )
    assert "set_init_sequence({240, 1, 8, 242" in main_cpp
    assert "show_test_card();" in main_cpp
    assert "set_write_only(true);" in main_cpp


def test_lvgl_generation(generate_main, get_path) -> None:
    main_cpp = generate_main(get_path("lvgl.yaml"))
    assert (
        "mipi_spi::MipiSpi<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_SINGLE, 128, 160, 0, 0>();"
        in main_cpp
    )
    assert "set_init_sequence({1, 0, 10, 255, 177" in main_cpp
    assert "show_test_card();" not in main_cpp
    assert "set_auto_clear(false);" in main_cpp
