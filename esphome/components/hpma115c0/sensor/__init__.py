import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

from .. import CONF_HPMA115C0_ID, Hpma115C0PollingComponent, hpma115c0_ns

CONF_AQI_2_5 = "aqi_2_5"
CONF_AQI_10_0 = "aqi_10_0"
UNIT_INDEX = "index"

Hpma115C0Sensor = hpma115c0_ns.class_(
    "Hpma115C0Sensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Hpma115C0Sensor),
            cv.GenerateID(CONF_HPMA115C0_ID): cv.use_id(Hpma115C0PollingComponent),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AQI_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_INDEX,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AQI_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_INDEX,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_4_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_4_0])
        cg.add(var.set_pm_4_0_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    if CONF_AQI_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_AQI_2_5])
        cg.add(var.set_aqi_2_5_sensor(sens))

    if CONF_AQI_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_AQI_10_0])
        cg.add(var.set_aqi_10_0_sensor(sens))
    Hpma115C0_polling_component = await cg.get_variable(config[CONF_HPMA115C0_ID])
    cg.add(Hpma115C0_polling_component.register_listener(var))
