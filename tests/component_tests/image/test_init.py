"""Tests for image configuration validation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.image import CONFIG_SCHEMA


def test_image_configuration_errors() -> None:
    """Test detection of invalid configuration."""
    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA("a string")
    assert "Badly formed image configuration, expected a list or a dictionary" in str(
        exc_info.value
    )

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA({"id": "image_id", "type": "rgb565"})
    assert "required key not provided @ data[0]['file']" in str(exc_info.value)

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA({"file": "image.png", "type": "rgb565"})
    assert "required key not provided @ data[0]['id']" in str(exc_info.value)

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA({"id": "mdi_id", "file": "mdi:weather-##", "type": "rgb565"})
    assert "Could not parse mdi icon name" in str(exc_info.value)

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "binary",
                "transparency": "alpha_channel",
            }
        )
    assert "Image format 'BINARY' cannot have transparency" in str(exc_info.value)

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "rgb565",
                "transparency": "chroma_key",
                "invert_alpha": True,
            }
        )
    assert "No alpha channel to invert" in str(exc_info.value)

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "binary",
                "byte_order": "big_endian",
            }
        )
    assert "Image format 'BINARY' does not support byte order configuration" in str(
        exc_info.value
    )

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA({"id": "image_id", "file": "bad.png", "type": "binary"})
    assert "File can't be opened as image" in str(exc_info.value)

    with pytest.raises(cv.Invalid) as exc_info:
        CONFIG_SCHEMA(
            {"defaults": {}, "images": [{"id": "image_id", "file": "image.png"}]}
        )
    assert "Type is required either in the image config or in the defaults" in str(
        exc_info.value
    )


def test_image_configuration_success() -> None:
    """Test successful configuration validation."""

    # Valid image configuration
    config1: dict[str, Any] = {
        "id": "image_id",
        "file": "image.png",
        "type": "rgb565",
        "transparency": "chroma_key",
        "byte_order": "little_endian",
        "dither": "FloydSteinberg",
        "resize": "100x100",
        "invert_alpha": False,
    }
    CONFIG_SCHEMA(config1)

    config2: list[dict[str, Any]] = [
        {
            "id": "image_id",
            "file": "image.png",
            "type": "binary",
        }
    ]
    CONFIG_SCHEMA(config2)

    config3: dict[str, Any] = {
        "defaults": {
            "type": "rgb565",
            "transparency": "chroma_key",
            "byte_order": "little_endian",
            "dither": "FloydSteinberg",
            "resize": "100x100",
            "invert_alpha": False,
        },
        "images": [
            {
                "id": "image_id",
                "file": "image.png",
            }
        ],
    }
    CONFIG_SCHEMA(config3)

    config4: dict[str, Any] = {
        "rgb565": {
            "alpha_channel": [
                {
                    "id": "image_id",
                    "file": "image.png",
                    "transparency": "alpha_channel",
                    "byte_order": "little_endian",
                    "dither": "FloydSteinberg",
                    "resize": "100x100",
                    "invert_alpha": False,
                }
            ]
        },
        "binary": [
            {
                "id": "image_id",
                "file": "image.png",
                "transparency": "opaque",
                "dither": "FloydSteinberg",
                "resize": "100x100",
                "invert_alpha": False,
            }
        ],
    }
    CONFIG_SCHEMA(config4)


def _get_path(file_name: str) -> Path:
    """Helper function to get the absolute path of a file relative to the test script."""
    return (Path(__file__).parent / file_name).absolute()


def test_image_generation(generate_main: Callable[[str | Path], str]) -> None:
    """Test image generation configuration."""

    main_cpp: str = generate_main(_get_path("image_test.yaml"))
    assert "uint8_t_id[] PROGMEM = {0x24, 0x21, 0x24, 0x21" in main_cpp
    assert (
        "cat_img = new image::Image(uint8_t_id, 32, 24, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);"
        in main_cpp
    )
