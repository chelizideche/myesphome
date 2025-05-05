import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_COLOR_TEMPERATURE,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_NAME,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_LUX,
    UNIT_PERCENT,
)

CODEOWNERS = ["@mrgnr", "@latonita"]
AUTO_LOAD = ["text_sensor"]
DEPENDENCIES = ["i2c", "sensor"]

as7343_ns = cg.esphome_ns.namespace("as7343")

AS7343Component = as7343_ns.class_(
    "AS7343Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_ATIME = "atime"
CONF_ASTEP = "astep"
CONF_DARK_CURRENT = "dark_current"
CONF_CHANNEL_CORRECTION = "channel_correction"

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

CONF_BAND_COUNTS = "band_counts"
CONF_BAND_BASIC_COUNTS = "band_basic_counts"

CONF_IRRADIANCE = "irradiance"
CONF_IRRADIANCE_PHOTOPIC = "irradiance_photopic"
CONF_PPFD = "ppfd"
CONF_IRRADIANCE_PAR = "irradiance_par"
CONF_SATURATION = "saturation"
CONF_SATURATION_LEVEL = "saturation_level"
CONF_RGB_HEX = "rgb_hex"

UNIT_COUNTS = "#"
UNIT_IRRADIANCE = "W/m²"
UNIT_PPFD = "µmol/s⋅m²"

ICON_BAND_COUNTS = "mdi:counter"
ICON_BAND_IRRADIANCE = "mdi:waveform"
ICON_IRRADIANCE = "mdi:radioactive"
ICON_ILLUMINANCE = "mdi:weather-sunny"
ICON_IRRADIANCE_PHOTOPIC = "mdi:sun-wireless-outline"
ICON_PAR = "mdi:sprout"
ICON_PPFD = "mdi:sprout-outline"
ICON_COLOR_TEMPERATURE = "mdi:sun-thermometer-outline"
ICON_SATURATION = "mdi:weather-sunny-alert"
ICON_PALETTE = "mdi:palette"


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


FLOAT_10_SCHEMA = cv.Schema(
    cv.All(
        cv.ensure_list(cv.float_),
        cv.Length(min=10, max=10),
    ),
)

CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHANNEL_CORRECTION, default=[1.0] * 13): FLOAT_10_SCHEMA,
        cv.Optional(CONF_DARK_CURRENT, default=[0.0] * 13): FLOAT_10_SCHEMA,
        cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_,
    }
)

COUNTS_SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_BAND_COUNTS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)

BASIC_COUNTS_SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_BAND_COUNTS,
        accuracy_decimals=6,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


BAND_COUNTS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_F1): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F2): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_FZ): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F3): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F4): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_FY): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F5): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_FXL): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F6): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F7): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F8): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_NIR): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_CLEAR): COUNTS_SENSOR_SCHEMA,
    }
)

BAND_BASIC_COUNTS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_F1): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F2): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_FZ): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F3): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F4): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_FY): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F5): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_FXL): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F6): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F7): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F8): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_NIR): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_CLEAR): BASIC_COUNTS_SENSOR_SCHEMA,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS7343Component),
            cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
            cv.Optional(CONF_BAND_COUNTS): BAND_COUNTS_SCHEMA,
            cv.Optional(CONF_BAND_BASIC_COUNTS): BAND_BASIC_COUNTS_SCHEMA,
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
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_IRRADIANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE_PHOTOPIC): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_IRRADIANCE_PHOTOPIC,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_IRRADIANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PPFD): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PPFD,
                    icon=ICON_PPFD,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_IRRADIANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE_PAR): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_PAR,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_IRRADIANCE,
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
            cv.Optional(CONF_SATURATION_LEVEL): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PERCENT,
                    icon=ICON_SATURATION,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_RGB_HEX): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(
                    icon=ICON_PALETTE,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)

# "F1",  "F2", "FZ", "F3", "F4",  "FY",    "F5",
# "FXL", "F6", "F7", "F8", "NIR", "Clear", "*"};
BANDS = {
    CONF_F1: 0,
    CONF_F2: 1,
    CONF_FZ: 2,
    CONF_F3: 3,
    CONF_F4: 4,
    CONF_FY: 5,
    CONF_F5: 6,
    CONF_FXL: 7,
    CONF_F6: 8,
    CONF_F7: 9,
    CONF_F8: 10,
    CONF_NIR: 11,
    CONF_CLEAR: 12,
}

SENSORS_INTEGRAL = [
    CONF_ILLUMINANCE,
    CONF_IRRADIANCE,
    CONF_IRRADIANCE_PHOTOPIC,
    CONF_IRRADIANCE_PAR,
    CONF_PPFD,
    CONF_COLOR_TEMPERATURE,
    CONF_SATURATION,
    CONF_SATURATION_LEVEL,
]


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_atime(config[CONF_ATIME]))
    cg.add(var.set_astep(config[CONF_ASTEP]))

    if calibration := config.get(CONF_CALIBRATION):
        cg.add(var.set_dark_current_calibration(calibration[CONF_DARK_CURRENT]))
        cg.add(var.set_channel_correction(calibration[CONF_CHANNEL_CORRECTION]))
        cg.add(
            var.set_glass_attenuation_factor(calibration[CONF_GLASS_ATTENUATION_FACTOR])
        )

    if counts_config := config.get(CONF_BAND_COUNTS):
        for key, ch in BANDS.items():
            if sensor_config := counts_config.get(key):
                sens = await sensor.new_sensor(sensor_config)
                cg.add(getattr(var, "set_band_counts_sensor")(sens, ch))

    if basic_counts_config := config.get(CONF_BAND_BASIC_COUNTS):
        for key, ch in BANDS.items():
            if sensor_config := basic_counts_config.get(key):
                sens = await sensor.new_sensor(sensor_config)
                cg.add(getattr(var, "set_band_basic_counts_sensor")(sens, ch))

    for sensor_id in SENSORS_INTEGRAL:
        if sensor_config := config.get(sensor_id):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{sensor_id}_sensor")(sens))

    if rgb_config := config.get(CONF_RGB_HEX):
        sens = await text_sensor.new_text_sensor(rgb_config)
        cg.add(var.set_rgb_hex_sensor(sens))
