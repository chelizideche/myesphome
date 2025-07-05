import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_WATT_HOURS,
)

from .. import CONF_TAG_NAME, CONF_TELEINFO_ID, TeleInfo, teleinfo_ns

TeleInfoSensor = teleinfo_ns.class_("TeleInfoSensor", sensor.Sensor, cg.Component)


# Define sensor type configurations by prefix
TIC_TAG_CONFIGS = {
    # Energy tags (Wh) - All tags starting with EA
    "EA": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_WATT_HOURS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_ACCURACY_DECIMALS: 0,
    },
    # Current tags (A)
    "IRMS": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_AMPERE,
        CONF_DEVICE_CLASS: DEVICE_CLASS_CURRENT,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 0,
    },
    # Voltage tags (V)
    "U": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT,
        CONF_DEVICE_CLASS: DEVICE_CLASS_VOLTAGE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "SINST": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT_AMPS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_POWER,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "SMAX": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT_AMPS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_POWER,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 0,
    },
}


# Create a base schema that's flexible for any tag
BASE_SCHEMA = sensor.sensor_schema(
    TeleInfoSensor,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
).extend(
    {
        cv.GenerateID(CONF_TELEINFO_ID): cv.use_id(TeleInfo),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


def apply_tag_defaults(config):
    """Apply defaults based on tag prefix for TIC standard mode tags"""
    tag = config[CONF_TAG_NAME]

    # Skip if tag is too short
    if len(tag) < 2:
        return config

    tag_upper = tag.upper()

    # Check all patterns by prefix
    for prefix, prefix_config in TIC_TAG_CONFIGS.items():
        if tag_upper.startswith(prefix):
            # Apply prefix defaults if not overridden by user
            for key, value in prefix_config.items():
                if key not in config:
                    config[key] = value
            return config

    return config


# Apply tag defaults during schema validation
CONFIG_SCHEMA = cv.All(BASE_SCHEMA, apply_tag_defaults)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    teleinfo = await cg.get_variable(config[CONF_TELEINFO_ID])
    cg.add(teleinfo.register_teleinfo_listener(var))
    cg.add(teleinfo.register_sensor(config[CONF_TAG_NAME], var))
