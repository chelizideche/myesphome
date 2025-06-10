import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
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


# First define the base schema with mandatory EmonTx listener fields
BASE_SCHEMA = {
    cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
    cv.Required(CONF_TAG_NAME): cv.string,
}


# Create a schema for each sensor type
def create_sensor_schema(tag):
    """Create appropriate schema based on tag pattern"""
    tag_upper = tag.upper()

    # Power sensors (P1, P2, P3, etc.)
    if tag_upper.startswith("P") and len(tag_upper) >= 2 and len(tag_upper) <= 3:
        return sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(BASE_SCHEMA)

    # Energy sensors (E1, E2, E3, etc.)
    if tag_upper.startswith("E") and len(tag_upper) >= 2 and len(tag_upper) <= 3:
        return sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            accuracy_decimals=0,
        ).extend(BASE_SCHEMA)

    # Voltage sensors
    if tag_upper.startswith("VRMS") and len(tag_upper) == 5:
        return sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=2,
        ).extend(BASE_SCHEMA)

    # Temperature sensors (T1, T2, etc.)
    if tag_upper.startswith("T") and len(tag_upper) == 2:
        return sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(BASE_SCHEMA)

    # Default for all other sensors
    return sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
    ).extend(BASE_SCHEMA)


# Use a multi schema validator to select the right schema based on the tag
CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TAG_NAME: create_sensor_schema,
    }
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_EMONTX_ID])

    # Register the sensor with the parent component
    cg.add(parent.register_sensor(config[CONF_TAG_NAME], var))
