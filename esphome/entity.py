"""Entity-related helper functions."""

import logging

from esphome.const import (
    CONF_DEVICE_ID,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_INTERNAL,
    CONF_NAME,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, add, get_variable
from esphome.helpers import fnv1a_32bit_hash, sanitize, snake_case
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)


def get_base_entity_object_id(
    name: str, friendly_name: str | None, device_name: str | None = None
) -> str:
    """Calculate the base object ID for an entity that will be set via set_object_id().

    This function calculates what object_id_c_str_ should be set to in C++.

    The C++ EntityBase::get_object_id() (entity_base.cpp lines 38-49) works as:
    - If !has_own_name && is_name_add_mac_suffix_enabled():
        return str_sanitize(str_snake_case(App.get_friendly_name()))  // Dynamic
    - Else:
        return object_id_c_str_ ?? ""  // What we set via set_object_id()

    Since we're calculating what to pass to set_object_id(), we always need to
    generate the object_id the same way, regardless of name_add_mac_suffix setting.

    Args:
        name: The entity name (empty string if no name)
        friendly_name: The friendly name from CORE.friendly_name
        device_name: The device name if entity is on a sub-device

    Returns:
        The base object ID to use for duplicate checking and to pass to set_object_id()
    """

    if name:
        # Entity has its own name (has_own_name will be true)
        base_str = name
    elif device_name:
        # Entity has empty name and is on a sub-device
        # C++ EntityBase::set_name() uses device->get_name() when device is set
        base_str = device_name
    elif friendly_name:
        # Entity has empty name (has_own_name will be false)
        # C++ uses App.get_friendly_name() which returns friendly_name or device name
        base_str = friendly_name
    else:
        # Fallback to device name
        base_str = CORE.name

    return sanitize(snake_case(base_str))


async def setup_entity(var: MockObj, config: ConfigType, platform: str) -> None:
    """Set up generic properties of an Entity.

    This function handles duplicate entity names by automatically appending
    a suffix (_2, _3, etc.) when multiple entities have the same object_id
    within the same platform and device combination.

    Args:
        var: The entity variable to set up
        config: Configuration dictionary containing entity settings
        platform: The platform name (e.g., "sensor", "binary_sensor")
    """
    # Get device info
    device_id: int = 0
    device_name: str | None = None
    if CONF_DEVICE_ID in config:
        device_id_obj: ID = config[CONF_DEVICE_ID]
        device: MockObj = await get_variable(device_id_obj)
        add(var.set_device(device))
        # Use the device's ID hash as device_id

        device_id = fnv1a_32bit_hash(device_id_obj.id)
        # Get device name for object ID calculation
        device_name = device_id_obj.id

    add(var.set_name(config[CONF_NAME]))

    # Calculate base object_id using the same logic as C++
    # This must match the C++ behavior in esphome/core/entity_base.cpp
    base_object_id = get_base_entity_object_id(
        config[CONF_NAME], CORE.friendly_name, device_name
    )

    if not config[CONF_NAME]:
        _LOGGER.debug(
            "Entity has empty name, using '%s' as object_id base", base_object_id
        )

    # Handle duplicates
    # Check for duplicates
    unique_key: tuple[int, str, str] = (device_id, platform, base_object_id)
    if unique_key in CORE.unique_ids:
        # Found duplicate, add suffix
        count = CORE.unique_ids[unique_key] + 1
        CORE.unique_ids[unique_key] = count
        object_id = f"{base_object_id}_{count}"
        _LOGGER.info(
            "Duplicate %s entity '%s' found. Renaming to '%s'",
            platform,
            config[CONF_NAME],
            object_id,
        )
    else:
        # First occurrence
        CORE.unique_ids[unique_key] = 1
        object_id = base_object_id

    add(var.set_object_id(object_id))
    _LOGGER.debug(
        "Setting object_id '%s' for entity '%s' on platform '%s'",
        object_id,
        config[CONF_NAME],
        platform,
    )
    add(var.set_disabled_by_default(config[CONF_DISABLED_BY_DEFAULT]))
    if CONF_INTERNAL in config:
        add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        add(var.set_icon(config[CONF_ICON]))
    if CONF_ENTITY_CATEGORY in config:
        add(var.set_entity_category(config[CONF_ENTITY_CATEGORY]))
