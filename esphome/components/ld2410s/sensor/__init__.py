import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_EMPTY,
    UNIT_CENTIMETER,
    UNIT_PERCENT,
)

from .. import CONF_LD2410S_ID, LD2410S, ld2410s_ns

CODEOWNERS = ["@NovakIrs"]

LD2410SSensor = ld2410s_ns.class_("LD2410SSensor", sensor.Sensor, cg.Component)

TARGET_DISTANCE = "target_distance"
THRESHOLD_UPDATE = "threshold_update"

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LD2410SSensor),
            cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
            cv.Optional(TARGET_DISTANCE): sensor.sensor_schema(
                device_class=DEVICE_CLASS_DISTANCE,
                unit_of_measurement=UNIT_CENTIMETER,
                icon="mdi:arrow-left-right",
            ),
            cv.Optional(THRESHOLD_UPDATE): sensor.sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
                unit_of_measurement=UNIT_PERCENT,
                icon="mdi:percent",
            ),
        },
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if TARGET_DISTANCE in config:
        sens = await sensor.new_sensor(config[TARGET_DISTANCE])
        cg.add(var.set_distance_sensor(sens))
    if THRESHOLD_UPDATE in config:
        sens = await sensor.new_sensor(config[THRESHOLD_UPDATE])
        cg.add(var.set_threshold_update_sensor(sens))
    ld2410s = await cg.get_variable(config[CONF_LD2410S_ID])
    cg.add(ld2410s.register_listener(var))
