import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PASCAL,
    ICON_THERMOMETER,
    ICON_GAUGE,
)

DEPENDENCIES = ["i2c"]

m3200_ns = cg.esphome_ns.namespace("m3200")

CONF_PRESSURE_TYPE = "pressure_type"
CONF_FULL_SCALE_PRESSURE = "full_scale_pressure"
UNIT_PSI = "psi"

PRESSURE_GAGE_TYPE = {
    "GAGE": 0,
    "GAUGE": 0,
    "COMPOUND": -14.7,
}



m3200 = m3200_ns.class_("M3200", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(m3200),
            cv.Required(CONF_FULL_SCALE_PRESSURE): cv.int_range(min=10, max=6000),
            cv.Required(CONF_PRESSURE_TYPE): cv.enum(PRESSURE_GAGE_TYPE, upper=True),
            
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PSI,
                icon=ICON_GAUGE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_fs_pressure(config.get(CONF_FULL_SCALE_PRESSURE)))
    cg.add(var.set_min_pressure_(PRESSURE_GAGE_TYPE[config.get(CONF_PRESSURE_TYPE)]))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure_sensor(sens))
