"""Test get_base_entity_object_id function matches C++ behavior."""

from collections.abc import Generator
import re
from typing import Any

import pytest

from esphome import entity
from esphome.const import CONF_DEVICE_ID, CONF_DISABLED_BY_DEFAULT, CONF_ICON, CONF_NAME
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj
from esphome.entity import get_base_entity_object_id, setup_entity
from esphome.helpers import sanitize, snake_case

# Pre-compiled regex pattern for extracting object IDs from expressions
OBJECT_ID_PATTERN = re.compile(r'\.set_object_id\(["\'](.*?)["\']\)')


@pytest.fixture(autouse=True)
def restore_core_state() -> Generator[None, None, None]:
    """Save and restore CORE state for tests."""
    original_name = CORE.name
    original_friendly_name = CORE.friendly_name
    yield
    CORE.name = original_name
    CORE.friendly_name = original_friendly_name


def test_with_entity_name() -> None:
    """Test when entity has its own name - should use entity name."""
    # Simple name
    assert get_base_entity_object_id("Temperature Sensor", None) == "temperature_sensor"
    assert (
        get_base_entity_object_id("Temperature Sensor", "Device Name")
        == "temperature_sensor"
    )
    # Even with device name, entity name takes precedence
    assert (
        get_base_entity_object_id("Temperature Sensor", "Device Name", "Sub Device")
        == "temperature_sensor"
    )

    # Name with special characters
    assert (
        get_base_entity_object_id("Temp!@#$%^&*()Sensor", None)
        == "temp__________sensor"
    )
    assert get_base_entity_object_id("Temp-Sensor_123", None) == "temp-sensor_123"

    # Already snake_case
    assert get_base_entity_object_id("temperature_sensor", None) == "temperature_sensor"

    # Mixed case
    assert get_base_entity_object_id("TemperatureSensor", None) == "temperaturesensor"
    assert get_base_entity_object_id("TEMPERATURE SENSOR", None) == "temperature_sensor"


def test_empty_name_with_device_name() -> None:
    """Test when entity has empty name and is on a sub-device - should use device name."""
    # C++ behavior: when has_own_name is false and device is set, uses device->get_name()
    assert (
        get_base_entity_object_id("", "Friendly Device", "Sub Device 1")
        == "sub_device_1"
    )
    assert (
        get_base_entity_object_id("", "Kitchen Controller", "controller_1")
        == "controller_1"
    )
    assert get_base_entity_object_id("", None, "Test-Device_123") == "test-device_123"


def test_empty_name_with_friendly_name() -> None:
    """Test when entity has empty name and no device - should use friendly name."""
    # C++ behavior: when has_own_name is false, uses App.get_friendly_name()
    assert get_base_entity_object_id("", "Friendly Device") == "friendly_device"
    assert get_base_entity_object_id("", "Kitchen Controller") == "kitchen_controller"
    assert get_base_entity_object_id("", "Test-Device_123") == "test-device_123"

    # Special characters in friendly name
    assert get_base_entity_object_id("", "Device!@#$%") == "device_____"


def test_empty_name_no_friendly_name() -> None:
    """Test when entity has empty name and no friendly name - should use device name."""
    # Test with CORE.name set
    CORE.name = "device-name"
    assert get_base_entity_object_id("", None) == "device-name"

    CORE.name = "Test Device"
    assert get_base_entity_object_id("", None) == "test_device"


def test_edge_cases() -> None:
    """Test edge cases."""
    # Only spaces
    assert get_base_entity_object_id("   ", None) == "___"

    # Unicode characters (should be replaced)
    assert get_base_entity_object_id("Température", None) == "temp_rature"
    assert get_base_entity_object_id("测试", None) == "__"

    # Empty string with empty friendly name (empty friendly name is treated as None)
    # Falls back to CORE.name
    CORE.name = "device"
    assert get_base_entity_object_id("", "") == "device"

    # Very long name (should work fine)
    long_name = "a" * 100 + " " + "b" * 100
    expected = "a" * 100 + "_" + "b" * 100
    assert get_base_entity_object_id(long_name, None) == expected


@pytest.mark.parametrize(
    ("name", "expected"),
    [
        ("Temperature Sensor", "temperature_sensor"),
        ("Living Room Light", "living_room_light"),
        ("Test-Device_123", "test-device_123"),
        ("Special!@#Chars", "special___chars"),
        ("UPPERCASE NAME", "uppercase_name"),
        ("lowercase name", "lowercase_name"),
        ("Mixed Case Name", "mixed_case_name"),
        ("   Spaces   ", "___spaces___"),
    ],
)
def test_matches_cpp_helpers(name: str, expected: str) -> None:
    """Test that the logic matches using snake_case and sanitize directly."""
    # For non-empty names, verify our function produces same result as direct snake_case + sanitize
    assert get_base_entity_object_id(name, None) == sanitize(snake_case(name))
    assert get_base_entity_object_id(name, None) == expected


def test_empty_name_fallback() -> None:
    """Test empty name handling which falls back to friendly_name or CORE.name."""
    # Empty name is handled specially - it doesn't just use sanitize(snake_case(""))
    # Instead it falls back to friendly_name or CORE.name
    assert sanitize(snake_case("")) == ""  # Direct conversion gives empty string
    # But our function returns a fallback
    CORE.name = "device"
    assert get_base_entity_object_id("", None) == "device"  # Uses device name


def test_name_add_mac_suffix_behavior() -> None:
    """Test behavior related to name_add_mac_suffix.

    In C++, when name_add_mac_suffix is enabled and entity has no name,
    get_object_id() returns str_sanitize(str_snake_case(App.get_friendly_name()))
    dynamically. Our function always returns the same result since we're
    calculating the base for duplicate tracking.
    """
    # The function should always return the same result regardless of
    # name_add_mac_suffix setting, as we're calculating the base object_id
    assert get_base_entity_object_id("", "Test Device") == "test_device"
    assert get_base_entity_object_id("Entity Name", "Test Device") == "entity_name"


def test_priority_order() -> None:
    """Test the priority order: entity name > device name > friendly name > CORE.name."""
    CORE.name = "core-device"

    # 1. Entity name has highest priority
    assert (
        get_base_entity_object_id("Entity Name", "Friendly Name", "Device Name")
        == "entity_name"
    )

    # 2. Device name is next priority (when entity name is empty)
    assert (
        get_base_entity_object_id("", "Friendly Name", "Device Name") == "device_name"
    )

    # 3. Friendly name is next (when entity and device names are empty)
    assert get_base_entity_object_id("", "Friendly Name", None) == "friendly_name"

    # 4. CORE.name is last resort
    assert get_base_entity_object_id("", None, None) == "core-device"


@pytest.mark.parametrize(
    ("name", "friendly_name", "device_name", "expected"),
    [
        # name, friendly_name, device_name, expected
        ("Living Room Light", None, None, "living_room_light"),
        ("", "Kitchen Controller", None, "kitchen_controller"),
        (
            "",
            "ESP32 Device",
            "controller_1",
            "controller_1",
        ),  # Device name takes precedence
        ("GPIO2 Button", None, None, "gpio2_button"),
        ("WiFi Signal", "My Device", None, "wifi_signal"),
        ("", None, "esp32_node", "esp32_node"),
        ("Front Door Sensor", "Home Assistant", "door_controller", "front_door_sensor"),
    ],
)
def test_real_world_examples(
    name: str, friendly_name: str | None, device_name: str | None, expected: str
) -> None:
    """Test real-world entity naming scenarios."""
    result = get_base_entity_object_id(name, friendly_name, device_name)
    assert result == expected


def test_issue_6953_scenarios() -> None:
    """Test specific scenarios from issue #6953."""
    # Scenario 1: Multiple empty names on main device with name_add_mac_suffix
    # The Python code calculates the base, C++ might append MAC suffix dynamically
    CORE.name = "device-name"
    CORE.friendly_name = "Friendly Device"

    # All empty names should resolve to same base
    assert get_base_entity_object_id("", CORE.friendly_name) == "friendly_device"
    assert get_base_entity_object_id("", CORE.friendly_name) == "friendly_device"
    assert get_base_entity_object_id("", CORE.friendly_name) == "friendly_device"

    # Scenario 2: Empty names on sub-devices
    assert (
        get_base_entity_object_id("", "Main Device", "controller_1") == "controller_1"
    )
    assert (
        get_base_entity_object_id("", "Main Device", "controller_2") == "controller_2"
    )

    # Scenario 3: xyz duplicates
    assert get_base_entity_object_id("xyz", None) == "xyz"
    assert get_base_entity_object_id("xyz", "Device") == "xyz"


# Tests for setup_entity function


@pytest.fixture
def setup_test_environment() -> Generator[list[str], None, None]:
    """Set up test environment for setup_entity tests."""
    # Set CORE state for tests
    CORE.name = "test-device"
    CORE.friendly_name = "Test Device"
    # Store original add function

    original_add = entity.add
    # Track what gets added
    added_expressions: list[str] = []

    def mock_add(expression: Any) -> Any:
        added_expressions.append(str(expression))
        return original_add(expression)

    # Patch add function in entity module
    entity.add = mock_add
    yield added_expressions
    # Clean up
    entity.add = original_add


def extract_object_id_from_expressions(expressions: list[str]) -> str | None:
    """Extract the object ID that was set from the generated expressions."""
    for expr in expressions:
        # Look for set_object_id calls with regex to handle various formats
        # Matches: var.set_object_id("temperature_2") or var.set_object_id('temperature_2')
        if match := OBJECT_ID_PATTERN.search(expr):
            return match.group(1)
    return None


@pytest.mark.asyncio
async def test_setup_entity_no_duplicates(setup_test_environment: list[str]) -> None:
    """Test setup_entity with unique names."""

    added_expressions = setup_test_environment

    # Create mock entities
    var1 = MockObj("sensor1")
    var2 = MockObj("sensor2")

    # Set up first entity
    config1 = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
    }
    await setup_entity(var1, config1, "sensor")

    # Get object ID from first entity
    object_id1 = extract_object_id_from_expressions(added_expressions)
    assert object_id1 == "temperature"

    # Clear for next entity
    added_expressions.clear()

    # Set up second entity with different name
    config2 = {
        CONF_NAME: "Humidity",
        CONF_DISABLED_BY_DEFAULT: False,
    }
    await setup_entity(var2, config2, "sensor")

    # Get object ID from second entity
    object_id2 = extract_object_id_from_expressions(added_expressions)
    assert object_id2 == "humidity"


@pytest.mark.asyncio
async def test_setup_entity_with_duplicates(setup_test_environment: list[str]) -> None:
    """Test setup_entity with duplicate names."""

    added_expressions = setup_test_environment

    # Create mock entities
    entities = [MockObj(f"sensor{i}") for i in range(4)]

    # Set up entities with same name
    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    object_ids: list[str] = []
    for var in entities:
        added_expressions.clear()
        await setup_entity(var, config, "sensor")
        object_id = extract_object_id_from_expressions(added_expressions)
        object_ids.append(object_id)

    # Check that object IDs were set with proper suffixes
    assert object_ids[0] == "temperature"
    assert object_ids[1] == "temperature_2"
    assert object_ids[2] == "temperature_3"
    assert object_ids[3] == "temperature_4"


@pytest.mark.asyncio
async def test_setup_entity_different_platforms(
    setup_test_environment: list[str],
) -> None:
    """Test that same name on different platforms doesn't conflict."""

    added_expressions = setup_test_environment

    # Create mock entities
    sensor = MockObj("sensor1")
    binary_sensor = MockObj("binary_sensor1")
    text_sensor = MockObj("text_sensor1")

    config = {
        CONF_NAME: "Status",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    # Set up entities on different platforms
    platforms = [
        (sensor, "sensor"),
        (binary_sensor, "binary_sensor"),
        (text_sensor, "text_sensor"),
    ]

    object_ids: list[str] = []
    for var, platform in platforms:
        added_expressions.clear()
        await setup_entity(var, config, platform)
        object_id = extract_object_id_from_expressions(added_expressions)
        object_ids.append(object_id)

    # All should get base object ID without suffix
    assert all(obj_id == "status" for obj_id in object_ids)


@pytest.fixture
def mock_get_variable() -> Generator[dict[ID, MockObj], None, None]:
    """Mock get_variable to return test devices."""
    devices = {}
    original_get_variable = entity.get_variable

    async def _mock_get_variable(device_id: ID) -> MockObj:
        if device_id in devices:
            return devices[device_id]
        return await original_get_variable(device_id)

    entity.get_variable = _mock_get_variable
    yield devices
    # Clean up
    entity.get_variable = original_get_variable


@pytest.mark.asyncio
async def test_setup_entity_with_devices(
    setup_test_environment: list[str], mock_get_variable: dict[ID, MockObj]
) -> None:
    """Test that same name on different devices doesn't conflict."""
    added_expressions = setup_test_environment

    # Create mock devices
    device1_id = ID("device1", type="Device")
    device2_id = ID("device2", type="Device")
    device1 = MockObj("device1_obj")
    device2 = MockObj("device2_obj")

    # Register devices with the mock
    mock_get_variable[device1_id] = device1
    mock_get_variable[device2_id] = device2

    # Create sensors with same name on different devices
    sensor1 = MockObj("sensor1")
    sensor2 = MockObj("sensor2")

    config1 = {
        CONF_NAME: "Temperature",
        CONF_DEVICE_ID: device1_id,
        CONF_DISABLED_BY_DEFAULT: False,
    }

    config2 = {
        CONF_NAME: "Temperature",
        CONF_DEVICE_ID: device2_id,
        CONF_DISABLED_BY_DEFAULT: False,
    }

    # Get object IDs
    object_ids: list[str] = []
    for var, config in [(sensor1, config1), (sensor2, config2)]:
        added_expressions.clear()
        await setup_entity(var, config, "sensor")
        object_id = extract_object_id_from_expressions(added_expressions)
        object_ids.append(object_id)

    # Both should get base object ID without suffix (different devices)
    assert object_ids[0] == "temperature"
    assert object_ids[1] == "temperature"


@pytest.mark.asyncio
async def test_setup_entity_empty_name(setup_test_environment: list[str]) -> None:
    """Test setup_entity with empty entity name."""

    added_expressions = setup_test_environment

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await setup_entity(var, config, "sensor")

    object_id = extract_object_id_from_expressions(added_expressions)
    # Should use friendly name
    assert object_id == "test_device"


@pytest.mark.asyncio
async def test_setup_entity_empty_name_duplicates(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with multiple empty names."""

    added_expressions = setup_test_environment

    entities = [MockObj(f"sensor{i}") for i in range(3)]

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    object_ids: list[str] = []
    for var in entities:
        added_expressions.clear()
        await setup_entity(var, config, "sensor")
        object_id = extract_object_id_from_expressions(added_expressions)
        object_ids.append(object_id)

    # Should use device name with suffixes
    assert object_ids[0] == "test_device"
    assert object_ids[1] == "test_device_2"
    assert object_ids[2] == "test_device_3"


@pytest.mark.asyncio
async def test_setup_entity_special_characters(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with names containing special characters."""

    added_expressions = setup_test_environment

    entities = [MockObj(f"sensor{i}") for i in range(3)]

    config = {
        CONF_NAME: "Temperature Sensor!",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    object_ids: list[str] = []
    for var in entities:
        added_expressions.clear()
        await setup_entity(var, config, "sensor")
        object_id = extract_object_id_from_expressions(added_expressions)
        object_ids.append(object_id)

    # Special characters should be sanitized
    assert object_ids[0] == "temperature_sensor_"
    assert object_ids[1] == "temperature_sensor__2"
    assert object_ids[2] == "temperature_sensor__3"


@pytest.mark.asyncio
async def test_setup_entity_with_icon(setup_test_environment: list[str]) -> None:
    """Test setup_entity sets icon correctly."""

    added_expressions = setup_test_environment

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:thermometer",
    }

    await setup_entity(var, config, "sensor")

    # Check icon was set
    assert any(
        'sensor1.set_icon("mdi:thermometer")' in expr for expr in added_expressions
    )


@pytest.mark.asyncio
async def test_setup_entity_disabled_by_default(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity sets disabled_by_default correctly."""

    added_expressions = setup_test_environment

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: True,
    }

    await setup_entity(var, config, "sensor")

    # Check disabled_by_default was set
    assert any(
        "sensor1.set_disabled_by_default(true)" in expr for expr in added_expressions
    )


@pytest.mark.asyncio
async def test_setup_entity_mixed_duplicates(setup_test_environment: list[str]) -> None:
    """Test complex duplicate scenario with multiple platforms and devices."""

    added_expressions = setup_test_environment

    # Track results
    results: list[tuple[str, str]] = []

    # 3 sensors named "Status"
    for i in range(3):
        added_expressions.clear()
        var = MockObj(f"sensor_status_{i}")
        await setup_entity(
            var, {CONF_NAME: "Status", CONF_DISABLED_BY_DEFAULT: False}, "sensor"
        )
        object_id = extract_object_id_from_expressions(added_expressions)
        results.append(("sensor", object_id))

    # 2 binary_sensors named "Status"
    for i in range(2):
        added_expressions.clear()
        var = MockObj(f"binary_sensor_status_{i}")
        await setup_entity(
            var, {CONF_NAME: "Status", CONF_DISABLED_BY_DEFAULT: False}, "binary_sensor"
        )
        object_id = extract_object_id_from_expressions(added_expressions)
        results.append(("binary_sensor", object_id))

    # 1 text_sensor named "Status"
    added_expressions.clear()
    var = MockObj("text_sensor_status")
    await setup_entity(
        var, {CONF_NAME: "Status", CONF_DISABLED_BY_DEFAULT: False}, "text_sensor"
    )
    object_id = extract_object_id_from_expressions(added_expressions)
    results.append(("text_sensor", object_id))

    # Check results - each platform has its own namespace
    assert results[0] == ("sensor", "status")  # sensor
    assert results[1] == ("sensor", "status_2")  # sensor
    assert results[2] == ("sensor", "status_3")  # sensor
    assert results[3] == ("binary_sensor", "status")  # binary_sensor (new namespace)
    assert results[4] == ("binary_sensor", "status_2")  # binary_sensor
    assert results[5] == ("text_sensor", "status")  # text_sensor (new namespace)
