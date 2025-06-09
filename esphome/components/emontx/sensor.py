import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from . import CONF_EMONTX_ID, CONF_TAG_NAME, EmonTx

DEPENDENCIES = ["emontx"]


def get_sensor_defaults(tag):
    """
    Automatically determine sensor properties based on tag patterns.
    Returns a dictionary with default properties that can be overridden by user.
    """
    tag_upper = tag.upper()
    defaults = {}

    # Power sensors (P1, P2, P3, etc.)
    if tag_upper.startswith("P") and len(tag_upper) >= 2 and len(tag_upper) <= 3:
        defaults = {
            CONF_UNIT_OF_MEASUREMENT: UNIT_WATT,
            CONF_DEVICE_CLASS: DEVICE_CLASS_POWER,
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
            CONF_ACCURACY_DECIMALS: 1,
        }
    # Energy sensors (E1, E2, E3, etc.)
    elif tag_upper.startswith("E") and len(tag_upper) >= 2 and len(tag_upper) <= 3:
        defaults = {
            CONF_UNIT_OF_MEASUREMENT: UNIT_WATT_HOURS,
            CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
            CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
            CONF_ACCURACY_DECIMALS: 0,
        }
    # Voltage sensors (Vrms1, Vrms2, Vrms3, etc.)
    elif tag_upper.startswith("VRMS") and len(tag_upper) == 5:
        defaults = {
            CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT,
            CONF_DEVICE_CLASS: DEVICE_CLASS_VOLTAGE,
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
            CONF_ACCURACY_DECIMALS: 2,
        }
    # Temperature sensors (T1, T2, etc.)
    elif tag_upper.startswith("T") and len(tag_upper) == 2:
        defaults = {
            CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
            CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
            CONF_ACCURACY_DECIMALS: 1,
        }
    # Counter/pulse sensors (MSG, PULSE, COUNT, etc.)
    elif tag_upper in ["MSG", "PULSE", "ANALOG", "DIGPULSE", "ANAPULSE"]:
        defaults = {
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
            CONF_ACCURACY_DECIMALS: 0,
        }
    # Default for unknown patterns
    else:
        defaults = {
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
            CONF_ACCURACY_DECIMALS: 0,
        }

    return defaults


def apply_sensor_defaults(config):
    """Apply automatic defaults based on tag, but allow user overrides."""
    tag = config[CONF_TAG_NAME]
    defaults = get_sensor_defaults(tag)

    # Create a new config with defaults applied only if not already specified
    new_config = defaults.copy()
    new_config.update(config)  # User config takes precedence

    return new_config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema().extend(
        {
            cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
            cv.Required(CONF_TAG_NAME): cv.string,
        }
    ),
    apply_sensor_defaults,  # Apply defaults after schema validation
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_EMONTX_ID])

    # Register the sensor with the parent component
    cg.add(parent.register_sensor(config[CONF_TAG_NAME], var))
