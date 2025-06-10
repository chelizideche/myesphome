import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PULSES,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import CONF_EMONTX_ID, CONF_TAG_NAME, EmonTx, emontx_ns

EmonTxSensor = emontx_ns.class_("EmonTxSensor", sensor.Sensor, cg.Component)

# Define sensor type configurations by prefix
SENSOR_CONFIGS = {
    "P": {
        "unit_of_measurement": UNIT_WATT,
        "device_class": DEVICE_CLASS_POWER,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    "E": {
        "unit_of_measurement": UNIT_WATT_HOURS,
        "device_class": DEVICE_CLASS_ENERGY,
        "state_class": STATE_CLASS_TOTAL_INCREASING,
        "accuracy_decimals": 3,
    },
    "V": {
        "unit_of_measurement": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 2,
    },
    "I": {
        "unit_of_measurement": UNIT_AMPERE,
        "device_class": DEVICE_CLASS_CURRENT,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 2,
    },
    "T": {
        "unit_of_measurement": UNIT_CELSIUS,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
}

# Pattern-based configurations
PATTERN_CONFIGS = {
    "PULSE": {
        "unit_of_measurement": UNIT_PULSES,
        "accuracy_decimals": 0,
    },
    "PF": {
        "unit_of_measurement": UNIT_EMPTY,
        "device_class": DEVICE_CLASS_POWER_FACTOR,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 2,
    },
}

# Create a base schema that's flexible for any tag
BASE_SCHEMA = sensor.sensor_schema(
    EmonTxSensor,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
).extend(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


def apply_tag_defaults(config):
    """Apply defaults based on tag prefix if applicable, but don't restrict any tags"""
    tag = config[CONF_TAG_NAME]

    # Skip if tag is too short
    if len(tag) < 2:
        return config

    # Check if this tag starts with a known prefix
    tag_upper = tag.upper()

    for pattern, pattern_config in PATTERN_CONFIGS.items():
        if pattern == "PF" and tag_upper.startswith("PF"):
            # Apply pattern defaults if not overridden by user
            for key, value in pattern_config.items():
                if key not in config:
                    config[key] = value
            # Once matched a pattern, don't check prefixes
            return config
        if pattern.upper() in tag_upper:
            # Apply pattern defaults if not overridden by user
            for key, value in pattern_config.items():
                if key not in config:
                    config[key] = value
            # Once matched a pattern, don't check prefixes
            return config

    # Only apply defaults for known prefixes with numeric indices
    prefix = tag_upper[0]
    if prefix in SENSOR_CONFIGS and len(tag) > 1 and tag[1:].isdigit():
        # Apply defaults for known tag types, but only if not overridden by user
        defaults = SENSOR_CONFIGS[prefix]
        for key, value in defaults.items():
            if key not in config:
                config[key] = value

    return config


CONFIG_SCHEMA = cv.All(BASE_SCHEMA, apply_tag_defaults)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    emontx = await cg.get_variable(config[CONF_EMONTX_ID])
    cg.add(emontx.register_emontx_listener(var))
    cg.add(emontx.register_sensor(config[CONF_TAG_NAME], var))
