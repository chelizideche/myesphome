import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@DanBar1971"]

dfrobot_ns = cg.esphome_ns.namespace("dfrobot_air_quality_sensor")
DFRobotAirQualitySensor = dfrobot_ns.class_("DFRobotAirQualitySensor", cg.PollingComponent, i2c.I2CDevice)
AQICalculatorType = dfrobot_ns.enum("AQICalculatorType")

DEVICE_CLASS_PM03 = "pm03"
DEVICE_CLASS_PM05 = "pm05"
DEVICE_CLASS_PM50 = "pm50"
DEVICE_CLASS_PM105 = "pm105"

CONF_PM_1_0 = "pm_1_0"
CONF_PM_2_5 = "pm_2_5"
CONF_PM_10_0 = "pm_10"

CONF_PM_0_3UM = "pm_0_3um"
CONF_PM_0_5UM = "pm_0_5um"
CONF_PM_5_0UM = "pm_5_0um"
CONF_PM_10_5UM = "pm_10_5um"

CONF_AQI    = "aqi"
CONF_CALCULATION_TYPE = "calculation_type"

UNIT_INDEX = "index"

AQI_CALCULATION_TYPE = {
    "CAQI": AQICalculatorType.CAQI_TYPE,
    "AQI": AQICalculatorType.AQI_TYPE,
}

def _validate(config):
    if CONF_AQI in config and CONF_PM_2_5 not in config:
        raise cv.Invalid("AQI sensor requires PM 2.5")
    if CONF_AQI in config and CONF_PM_10_0 not in config:
        raise cv.Invalid("AQI sensor requires PM 10 sensors")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DFRobotAirQualitySensor),
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
        cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM10,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_0_3UM): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),    
        cv.Optional(CONF_PM_0_5UM): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),    
        cv.Optional(CONF_PM_5_0UM): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),    
        cv.Optional(CONF_PM_10_5UM): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),    
        cv.Optional(CONF_AQI): sensor.sensor_schema(
            unit_of_measurement=UNIT_INDEX,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_AQI,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Required(CONF_CALCULATION_TYPE): cv.enum(
                    AQI_CALCULATION_TYPE, upper=True
                ),
            }    
        ),
    }
).extend(i2c.i2c_device_schema(0x19)).extend(cv.polling_component_schema("60s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))
    
    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))
    
    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_sensor(sens))
    
    if CONF_PM_0_3UM in config:
        sens = await sensor.new_sensor(config[CONF_PM_0_3UM])
        cg.add(var.set_pm_0_3_sensor(sens))

    if CONF_PM_0_5UM in config:
        sens = await sensor.new_sensor(config[CONF_PM_0_5UM])
        cg.add(var.set_pm_0_5_sensor(sens))
    
    if CONF_PM_5_0UM in config:
        sens = await sensor.new_sensor(config[CONF_PM_5_0UM])
        cg.add(var.set_pm_5_0_sensor(sens))
    
    if CONF_PM_10_5UM in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_5UM])
        cg.add(var.set_pm_10_5_sensor(sens))
    
    if CONF_AQI in config:
        sens = await sensor.new_sensor(config[CONF_AQI])
        cg.add(var.set_aqi_sensor(sens))
        cg.add(var.set_aqi_calculation_type(config[CONF_AQI][CONF_CALCULATION_TYPE]))
        
    
    


