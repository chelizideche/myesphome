import esphome.codegen as cg
from esphome.components import i2c, display
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_BUFFER_SIZE,
    CONF_BRIGHTNESS,
    CONF_DEVICE,
    CONF_CONTINUOUS,
)

DEPENDENCIES = ["i2c"]

ht16k33_char_ns = cg.esphome_ns.namespace("ht16k33_char")

CONF_SCROLL = "scroll"
CONF_SCROLL_SPEED = "scroll_speed"
CONF_SCROLL_DWELL = "scroll_dwell"
CONF_SCROLL_DELAY = "scroll_delay"
CONF_SECONDARY_DISPLAYS = "secondary_displays"

CONFIG_SECONDARY = cv.Schema({cv.GenerateID(): cv.declare_id(i2c.I2CDevice)}).extend(
    i2c.i2c_device_schema(None)
)

HT16k33Char_BaseClassType = ht16k33_char_ns.class_(
    "HT16k33CharComponent", cg.PollingComponent, i2c.I2CDevice
)

# A dictionary for supported device types:
#  -The key is what the user would put in the YAML file to select this device.
#  -The value is a dictionary that contains the keys:
#     `CLASS_NAME`: The name of the class that implements the device.
HT16K33_DEVICE_TYPES = {
    "ADAFRUIT_7SEGMENT_1.2IN": {
        "CLASS_NAME": "Adafruit7SegLarge",
    },
    "ADAFRUIT_7SEGMENT_1.2IN_FLIPPED": {
        "CLASS_NAME": "Adafruit7SegLargeFlip",
    },
    "ADAFRUIT_7SEGMENT_.56IN": {
        "CLASS_NAME": "Adafruit7Seg",
    },
    "ADAFRUIT_7SEGMENT_.56IN_FLIPPED": {
        "CLASS_NAME": "Adafruit7SegFlip",
    },
    "ADAFRUIT_14_SEG": {
        "CLASS_NAME": "Adafruit14Seg",
    },
    "ADAFRUIT_14_SEG_FLIPPED": {
        "CLASS_NAME": "Adafruit14SegFlip",
    },
}

HT16k33Char_BaseClassTypeRef = HT16k33Char_BaseClassType.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HT16k33Char_BaseClassType),
            cv.Required(CONF_DEVICE): cv.enum(HT16K33_DEVICE_TYPES, upper=True),
            cv.Optional(CONF_BUFFER_SIZE, default=8): cv.int_range(min=4, max=255),
            cv.Optional(CONF_BRIGHTNESS, default=15): cv.int_range(min=1, max=16),
            cv.Optional(CONF_SECONDARY_DISPLAYS): cv.ensure_list(CONFIG_SECONDARY),
            cv.Optional(CONF_CONTINUOUS, default=False): cv.boolean,
            cv.Optional(CONF_SCROLL, default=False): cv.boolean,
            cv.Optional(
                CONF_SCROLL_SPEED, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SCROLL_DWELL, default="2s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SCROLL_DELAY, default="5s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x70))
)


async def to_code(config):
    ClassType = ht16k33_char_ns.class_(
        HT16K33_DEVICE_TYPES[config[CONF_DEVICE]]["CLASS_NAME"],
        HT16k33Char_BaseClassType,
    )
    ClassInstantiation = ClassType.new()
    var = cg.Pvariable(config[CONF_ID], ClassInstantiation, ClassType)

    await i2c.register_i2c_device(var, config)
    await display.register_display(var, config)
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(HT16k33Char_BaseClassTypeRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))

    if config[CONF_SCROLL]:
        cg.add(var.set_scroll(True))
        cg.add(var.set_continuous(config[CONF_CONTINUOUS]))
        cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))
        cg.add(var.set_scroll_dwell(config[CONF_SCROLL_DWELL]))
        cg.add(var.set_scroll_delay(config[CONF_SCROLL_DELAY]))

    if CONF_SECONDARY_DISPLAYS in config:
        for conf in config[CONF_SECONDARY_DISPLAYS]:
            disp = cg.new_Pvariable(conf[CONF_ID])
            await i2c.register_i2c_device(disp, conf)
            cg.add(var.add_secondary_display(disp))
