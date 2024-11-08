import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components import sensor               
from esphome.components import event                

from esphome.const import *

from . import BTHome

CONF_WINDOW = "window"          

DEPENDENCIES = ["bthome"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(BTHome),
        cv.Optional(CONF_WINDOW): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_WINDOW
        ),
        cv.Optional(CONF_MOTION): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOTION
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_WINDOW in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_WINDOW])
        cg.add(parent.set_window(sens))

    if CONF_MOTION in config:                                                           
        sens = await binary_sensor.new_binary_sensor(config[CONF_MOTION])               
        cg.add(parent.set_motion(sens))                                                 
