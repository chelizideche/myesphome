from esphome import pins
import esphome.codegen as cg
from esphome.components import display
from esphome.components.display import validate_rotation
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_ORDER,
    CONF_CS_PIN,
    CONF_DATA_PINS,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_FREQUENCY,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)

CONF_WR_PIN = "wr_pin"
CONF_RD_PIN = "rd_pin"
# CONF_PIXEL_MODE = "pixel_mode"
CONF_COLOR_DEPTH = "color_depth"


CODEOWNERS = ["@lylavoie"]
DEPENDENCIES = ["esp32"]


def AUTO_LOAD():
    return ["psram"]


i8080_ns = cg.esphome_ns.namespace("i8080")
I8080Display = i8080_ns.class_(
    "I8080Display",
    cg.PollingComponent,
    display.Display,
    display.DisplayBuffer,
)

MODELS = {
    "ST7789V": i8080_ns.class_("I8080ST7789V", I8080Display),
    "NT35510": i8080_ns.class_("I8080NT35510", I8080Display),
}

ColorOrder = display.display_ns.enum("ColorMode")
COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}

ColorDepth = i8080_ns.enum("ColorDepth")
COLOR_DEPTH = {
    "8BIT": ColorDepth.BITS_8,
    "16BIT": ColorDepth.BITS_16,
}


def _validate(config):
    if config[CONF_MODEL] == "ST7789V" and config.get(CONF_COLOR_DEPTH) == "8BIT":
        raise cv.Invalid("ST7789V chipset does not support 8-bit color depth.")
    return config


def _validate_data_pins(config):
    print(len(config))
    if len(config) != 8 and len(config) != 16:
        raise cv.Invalid("Chips need either 8 or 16 pin data bus.")
    return config


CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
    cv.only_with_esp_idf,
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I8080Display),
            cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True, space="_"),
            cv.Optional(CONF_DIMENSIONS): cv.Any(
                cv.dimensions,
                cv.Schema(
                    {
                        cv.Required(CONF_WIDTH): cv.int_,
                        cv.Required(CONF_HEIGHT): cv.int_,
                        cv.Optional(CONF_OFFSET_HEIGHT, default=0): cv.int_,
                        cv.Optional(CONF_OFFSET_WIDTH, default=0): cv.int_,
                    }
                ),
            ),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_WR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RD_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DATA_PINS): cv.All(
                [pins.gpio_output_pin_schema],
                _validate_data_pins,
            ),
            cv.Optional(CONF_COLOR_ORDER): cv.enum(COLOR_ORDERS),
            cv.Optional(CONF_COLOR_DEPTH): cv.enum(COLOR_DEPTH),
            cv.Required(CONF_INVERT_COLORS): cv.boolean,
            cv.Exclusive(CONF_ROTATION, CONF_ROTATION): validate_rotation,
            cv.Exclusive(CONF_TRANSFORM, CONF_ROTATION): cv.Schema(
                {
                    cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                }
            ),
            cv.Optional(CONF_FREQUENCY): cv.frequency,
        }
    ).extend(cv.polling_component_schema("1s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
)


async def to_code(config):
    rhs = MODELS[config[CONF_MODEL]].new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    await display.register_display(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs))

    if CONF_RD_PIN in config:
        rd = await cg.gpio_pin_expression(config[CONF_RD_PIN])
        cg.add(var.set_rd_pin(rd))

    wr = await cg.gpio_pin_expression(config[CONF_WR_PIN])
    cg.add(var.set_wr_pin(wr))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    data_pins = []
    for pin in config[CONF_DATA_PINS]:
        data_pins.append(await cg.gpio_pin_expression(pin))
    cg.add(var.set_data_pins(data_pins))

    if CONF_FREQUENCY in config:
        cg.add(var.set_pclk_hz(config[CONF_FREQUENCY]))

    if CONF_DIMENSIONS in config:
        dimensions = config[CONF_DIMENSIONS]
        if isinstance(dimensions, dict):
            cg.add(var.set_dimensions(dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]))
            cg.add(
                var.set_offsets(
                    dimensions[CONF_OFFSET_WIDTH], dimensions[CONF_OFFSET_HEIGHT]
                )
            )
        else:
            (width, height) = dimensions
            cg.add(var.set_dimensions(width, height))

    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    # if pixel_mode := config.get(CONF_PIXEL_MODE):
    #     cg.add(var.set_pixel_mode(pixel_mode))
    if CONF_COLOR_ORDER in config:
        cg.add(var.set_color_order(COLOR_ORDERS[config[CONF_COLOR_ORDER]]))
    if CONF_COLOR_DEPTH in config:
        cg.add(var.set_buffer_color_depth(COLOR_DEPTH[config[CONF_COLOR_DEPTH]]))
    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.set_swap_xy(transform[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
