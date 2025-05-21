import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    CONF_STATE,
)
from esphome.components.canbus import (
    CONF_CANBUS_ID,
)
from . import ESP32Can

DEPENDENCIES = ["canbus"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_CANBUS_ID): cv.use_id(ESP32Can),
    cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTICF
    ),
}

async def to_code(config):
    esp32can = await cg.get_variable(config[CONF_CANBUS_ID])
    if CONF_STATE in config:
        var = await text_sensor.new_text_sensor(config[CONF_STATE])
        func = getattr(esp32can, f"set_{CONF_STATE}_text_sensor")
        cg.add(func(var))
