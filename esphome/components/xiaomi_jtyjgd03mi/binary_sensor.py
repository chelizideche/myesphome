import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_SMOKE,
    DEVICE_CLASS_BUTTON,
    CONF_ID,
    CONF_BUTTON,
)
from . import XiaomiJTYJQD03MI

CONF_SMOKE = "smoke"
ICON_SMOKE_SENSOR = "mdi:smoke-sensor"
DEPENDENCIES = ["xiaomi_jtyjgd03mi"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(XiaomiJTYJQD03MI),
        cv.Optional(CONF_SMOKE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_SMOKE
        ),
        cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BUTTON
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if smoke_config := config.get(CONF_SMOKE):
        sens = await binary_sensor.new_binary_sensor(smoke_config)
        cg.add(parent.set_smoke(sens))

    if button_config := config.get(CONF_BUTTON):
        sens = await binary_sensor.new_binary_sensor(button_config)
        cg.add(parent.set_button(sens))
