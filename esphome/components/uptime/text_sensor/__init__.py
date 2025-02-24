import esphome.codegen as cg
from esphome.components import text_sensor, time
import esphome.config_validation as cv
from esphome.const import (
    CONF_TIME_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_TIMER,
)

TYPE_DURATION = "duration"
TYPE_TIMESTAMP = "timestamp"

uptime_ns = cg.esphome_ns.namespace("uptime")
UptimeDurationTextSensor = uptime_ns.class_(
    "UptimeDurationTextSensor", text_sensor.TextSensor, cg.PollingComponent
)
UptimeTimestampTextSensor = uptime_ns.class_(
    "UptimeTimestampTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_DURATION: text_sensor.text_sensor_schema(
            UptimeDurationTextSensor,
            icon=ICON_TIMER,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        TYPE_TIMESTAMP: cv.Schema(
            {
                cv.GenerateID(CONF_TIME_ID): cv.All(
                    cv.requires_component("time"), cv.use_id(time.RealTimeClock)
                ),
            }
        )
        .extend(
            text_sensor.text_sensor_schema(
                UptimeTimestampTextSensor,
                icon=ICON_TIMER,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            )
        )
        .extend(cv.COMPONENT_SCHEMA),
    },
    default_type=TYPE_DURATION,
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    if time_id_config := config.get(CONF_TIME_ID):
        time_id = await cg.get_variable(time_id_config)
        cg.add(var.set_time(time_id))
