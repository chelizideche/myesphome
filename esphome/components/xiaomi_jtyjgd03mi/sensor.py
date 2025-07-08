import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_BATTERY_LEVEL,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
)

from . import XiaomiJTYJQD03MI

DEPENDENCIES = ["xiaomi_jtyjgd03mi"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(XiaomiJTYJQD03MI),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if battery_level_config := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(battery_level_config)
        cg.add(parent.set_battery_level(sens))
