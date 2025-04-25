import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_NAME,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_LUX,
)

CODEOWNERS = ["@mrgnr", "@latonita"]
DEPENDENCIES = ["i2c", "sensor"]

as7343_ns = cg.esphome_ns.namespace("as7343")

AS7343Component = as7343_ns.class_(
    "AS7343Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_ATIME = "atime"
CONF_ASTEP = "astep"

CONF_F1 = "f1"
CONF_F2 = "f2"
CONF_FZ = "fz"
CONF_F3 = "f3"
CONF_F4 = "f4"
CONF_FY = "fy"
CONF_F5 = "f5"
CONF_FXL = "fxl"
CONF_F6 = "f6"
CONF_F7 = "f7"
CONF_F8 = "f8"
CONF_NIR = "nir"
CONF_CLEAR = "clear"

CONF_IRRADIANCE = "irradiance"
CONF_IRRADIANCE_PHOTOPIC = "irradiance_photopic"
CONF_PPFD = "ppfd"
CONF_PAR = "par"
CONF_SATURATION = "saturation"

UNIT_COUNTS = "#"
UNIT_IRRADIANCE = "W/m²"
UNIT_PPFD = "µmol/s⋅m²"

ICON_COUNTS = "mdi:waveform"
ICON_IRRADIANCE = "mdi:radioactive"
ICON_ILLUMINANCE = "mdi:weather-sunny"
ICON_IRRADIANCE_PHOTOPIC = "mdi:sun-wireless-outline"
ICON_PAR = "mdi:sprout"
ICON_PPFD = "mdi:sprout-outline"
ICON_COLOR_TEMPERATURE = "mdi:sun-thermometer-outline"
ICON_SATURATION = "mdi:weather-sunny-alert"

AS7343_GAIN = as7343_ns.enum("AS7343Gain")
GAIN_OPTIONS = {
    "X0.5": AS7343_GAIN.AS7343_GAIN_0_5X,
    "X1": AS7343_GAIN.AS7343_GAIN_1X,
    "X2": AS7343_GAIN.AS7343_GAIN_2X,
    "X4": AS7343_GAIN.AS7343_GAIN_4X,
    "X8": AS7343_GAIN.AS7343_GAIN_8X,
    "X16": AS7343_GAIN.AS7343_GAIN_16X,
    "X32": AS7343_GAIN.AS7343_GAIN_32X,
    "X64": AS7343_GAIN.AS7343_GAIN_64X,
    "X128": AS7343_GAIN.AS7343_GAIN_128X,
    "X256": AS7343_GAIN.AS7343_GAIN_256X,
    "X512": AS7343_GAIN.AS7343_GAIN_512X,
    "X1024": AS7343_GAIN.AS7343_GAIN_1024X,
    "X2048": AS7343_GAIN.AS7343_GAIN_2048X,
}


SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_COUNTS,
        accuracy_decimals=4,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS7343Component),
            cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
            cv.Optional(CONF_F1): SENSOR_SCHEMA,
            cv.Optional(CONF_F2): SENSOR_SCHEMA,
            cv.Optional(CONF_FZ): SENSOR_SCHEMA,
            cv.Optional(CONF_F3): SENSOR_SCHEMA,
            cv.Optional(CONF_F4): SENSOR_SCHEMA,
            cv.Optional(CONF_FY): SENSOR_SCHEMA,
            cv.Optional(CONF_F5): SENSOR_SCHEMA,
            cv.Optional(CONF_FXL): SENSOR_SCHEMA,
            cv.Optional(CONF_F6): SENSOR_SCHEMA,
            cv.Optional(CONF_F7): SENSOR_SCHEMA,
            cv.Optional(CONF_F8): SENSOR_SCHEMA,
            cv.Optional(CONF_NIR): SENSOR_SCHEMA,
            cv.Optional(CONF_CLEAR): SENSOR_SCHEMA,
            cv.Optional(CONF_ILLUMINANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_LUX,
                    icon=ICON_ILLUMINANCE,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_IRRADIANCE,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE_PHOTOPIC): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_IRRADIANCE_PHOTOPIC,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PPFD): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PPFD,
                    icon=ICON_PPFD,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PAR): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_PAR,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_COLOR_TEMPERATURE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_KELVIN,
                    icon=ICON_COLOR_TEMPERATURE,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SATURATION): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_COUNTS,
                    icon=ICON_SATURATION,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)

SENSORS = [
    CONF_F1,
    CONF_F2,
    CONF_FZ,
    CONF_F3,
    CONF_F4,
    CONF_FY,
    CONF_F5,
    CONF_FXL,
    CONF_F6,
    CONF_F7,
    CONF_F8,
    CONF_NIR,
    CONF_CLEAR,
    CONF_ILLUMINANCE,
    CONF_IRRADIANCE,
    CONF_IRRADIANCE_PHOTOPIC,
    CONF_PAR,
    CONF_PPFD,
    CONF_COLOR_TEMPERATURE,
    CONF_SATURATION,
]


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_atime(config[CONF_ATIME]))
    cg.add(var.set_astep(config[CONF_ASTEP]))
    cg.add(var.set_glass_attenuation_factor(config[CONF_GLASS_ATTENUATION_FACTOR]))

    # for conf_id, set_sensor_func in SENSORS.items():
    #     if sens_config := config.get(conf_id):
    #         sens = await sensor.new_sensor(sens_config)
    #         cg.add(getattr(var, set_sensor_func)(sens))
    for sensor_id in SENSORS:
        if sensor_config := config.get(sensor_id):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{sensor_id}_sensor")(sens))
