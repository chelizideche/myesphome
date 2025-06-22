"""Unit tests for core config functionality including areas and devices."""

from collections.abc import Callable
from pathlib import Path
from typing import Any
from unittest.mock import patch

import pytest

from esphome import config, config_validation as cv, yaml_util
from esphome.config import Config
from esphome.const import CONF_AREA, CONF_AREAS, CONF_DEVICES
from esphome.core import CORE
from esphome.core.config import Area, validate_area_config

FIXTURES_DIR = Path(__file__).parent.parent / "fixtures" / "core" / "config"


@pytest.fixture
def yaml_file(tmp_path: Path) -> Callable[[str], str]:
    """Create a temporary YAML file for testing."""

    def _yaml_file(content: str) -> str:
        yaml_path = tmp_path / "test.yaml"
        yaml_path.write_text(content)
        return str(yaml_path)

    return _yaml_file


@pytest.fixture(autouse=True)
def reset_core():
    """Reset CORE after each test."""
    yield
    CORE.reset()


def load_config_from_yaml(
    yaml_file: Callable[[str], str], yaml_content: str
) -> Config | None:
    """Load configuration from YAML content."""
    yaml_path = yaml_file(yaml_content)
    parsed_yaml = yaml_util.load_yaml(yaml_path)

    # Mock yaml_util.load_yaml to return our parsed content
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", yaml_path),
    ):
        return config.read_config({})


def load_config_from_fixture(
    yaml_file: Callable[[str], str], fixture_name: str
) -> Config | None:
    """Load configuration from a fixture file."""
    fixture_path = FIXTURES_DIR / fixture_name
    yaml_content = fixture_path.read_text()
    return load_config_from_yaml(yaml_file, yaml_content)


def test_validate_area_config_with_string() -> None:
    """Test that string area config is converted to structured format."""
    result: dict[str, Any] = validate_area_config("Living Room")

    assert isinstance(result, dict)
    assert "id" in result
    assert "name" in result
    assert result["name"] == "Living Room"
    # ID should be based on slugified name
    assert result["id"].id == "living_room"


def test_validate_area_config_with_dict() -> None:
    """Test that structured area config passes through unchanged."""
    area_id = cv.declare_id(Area)("test_area")
    input_config: dict[str, Any] = {
        "id": area_id,
        "name": "Test Area",
    }

    result: dict[str, Any] = validate_area_config(input_config)

    assert result == input_config
    assert result["id"] == area_id
    assert result["name"] == "Test Area"


def test_device_with_valid_area_id(yaml_file: Callable[[str], str]) -> None:
    """Test that device with valid area_id works correctly."""
    result = load_config_from_fixture(yaml_file, "valid_area_device.yaml")
    assert result is not None

    esphome_config = result["esphome"]

    # Verify areas were parsed correctly
    assert CONF_AREAS in esphome_config
    areas = esphome_config[CONF_AREAS]
    assert len(areas) == 1
    assert areas[0]["id"].id == "bedroom_area"
    assert areas[0]["name"] == "Bedroom"

    # Verify devices were parsed correctly
    assert CONF_DEVICES in esphome_config
    devices = esphome_config[CONF_DEVICES]
    assert len(devices) == 1
    assert devices[0]["id"].id == "test_device"
    assert devices[0]["name"] == "Test Device"
    assert devices[0]["area_id"].id == "bedroom_area"


def test_multiple_areas_and_devices(yaml_file: Callable[[str], str]) -> None:
    """Test multiple areas and devices configuration."""
    result = load_config_from_fixture(yaml_file, "multiple_areas_devices.yaml")
    assert result is not None

    esphome_config = result["esphome"]

    # Verify main area
    assert CONF_AREA in esphome_config
    main_area = esphome_config[CONF_AREA]
    assert main_area["id"].id == "main_area"
    assert main_area["name"] == "Main Area"

    # Verify additional areas
    assert CONF_AREAS in esphome_config
    areas = esphome_config[CONF_AREAS]
    assert len(areas) == 2
    area_ids = {area["id"].id for area in areas}
    assert area_ids == {"area1", "area2"}

    # Verify devices
    assert CONF_DEVICES in esphome_config
    devices = esphome_config[CONF_DEVICES]
    assert len(devices) == 3

    # Check device-area associations
    device_area_map = {dev["id"].id: dev["area_id"].id for dev in devices}
    assert device_area_map == {
        "device1": "main_area",
        "device2": "area1",
        "device3": "area2",
    }


def test_legacy_string_area(
    yaml_file: Callable[[str], str], caplog: pytest.LogCaptureFixture
) -> None:
    """Test legacy string area configuration with deprecation warning."""
    result = load_config_from_fixture(yaml_file, "legacy_string_area.yaml")
    assert result is not None

    esphome_config = result["esphome"]

    # Verify the string was converted to structured format
    assert CONF_AREA in esphome_config
    area = esphome_config[CONF_AREA]
    assert isinstance(area, dict)
    assert area["name"] == "Living Room"
    assert area["id"].id == "living_room"

    # Check for deprecation warning
    assert "Using 'area' as a string is deprecated" in caplog.text


def test_area_id_collision(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that duplicate area IDs are detected."""
    result = load_config_from_fixture(yaml_file, "area_id_collision.yaml")
    assert result is None

    # Check for the specific error message in stdout
    captured = capsys.readouterr()
    # Exact duplicates are now caught by IDPassValidationStep
    assert "ID duplicate_id redefined! Check esphome->area->id." in captured.out


def test_device_without_area(yaml_file: Callable[[str], str]) -> None:
    """Test that devices without area_id work correctly."""
    result = load_config_from_fixture(yaml_file, "device_without_area.yaml")
    assert result is not None

    esphome_config = result["esphome"]

    # Verify device was parsed
    assert CONF_DEVICES in esphome_config
    devices = esphome_config[CONF_DEVICES]
    assert len(devices) == 1

    device = devices[0]
    assert device["id"].id == "test_device"
    assert device["name"] == "Test Device"

    # Verify no area_id is present
    assert "area_id" not in device


def test_device_with_invalid_area_id(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that device with non-existent area_id fails validation."""
    result = load_config_from_fixture(yaml_file, "device_invalid_area.yaml")
    assert result is None

    # Check for the specific error message in stdout
    captured = capsys.readouterr()
    assert (
        "Device 'Test Device' has an area_id 'nonexistent_area' that does not exist."
        in captured.out
    )


def test_device_id_hash_collision(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that device IDs with hash collisions are detected."""
    result = load_config_from_fixture(yaml_file, "device_id_collision.yaml")
    assert result is None

    # Check for the specific error message about hash collision
    captured = capsys.readouterr()
    # The error message shows the ID that collides and includes the hash value
    assert (
        "Device ID 'd6ka' with hash 3082558663 collides with existing device ID 'test_2258'"
        in captured.out
    )


def test_area_id_hash_collision(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that area IDs with hash collisions are detected."""
    result = load_config_from_fixture(yaml_file, "area_id_hash_collision.yaml")
    assert result is None

    # Check for the specific error message about hash collision
    captured = capsys.readouterr()
    # The error message shows the ID that collides and includes the hash value
    assert (
        "Area ID 'd6ka' with hash 3082558663 collides with existing area ID 'test_2258'"
        in captured.out
    )


def test_device_duplicate_id(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that duplicate device IDs are detected by IDPassValidationStep."""
    result = load_config_from_fixture(yaml_file, "device_duplicate_id.yaml")
    assert result is None

    # Check for the specific error message from IDPassValidationStep
    captured = capsys.readouterr()
    assert "ID duplicate_device redefined!" in captured.out
