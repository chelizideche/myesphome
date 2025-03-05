from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.spi import TYPE_OCTAL, TYPE_QUAD, TYPE_SINGLE
import esphome.config_validation as cv
from esphome.config_validation import ALLOW_EXTRA
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_COLOR_ORDER,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_ENABLE_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_RESET_PIN,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.core import TimePeriod

from . import CONF_DRAW_FROM_ORIGIN, CONF_DRAW_ROUNDING, MODE_BGR, MODE_RGB
from .models import DELAY_FLAG, DriverChip, amoled, ili, jc

DEPENDENCIES = ["spi"]

mipi_dbi_ns = cg.esphome_ns.namespace("mipi_dbi")
MIPI_DBI = mipi_dbi_ns.class_(
    "MipiDbi", display.Display, display.DisplayBuffer, cg.Component, spi.SPIDevice
)
ColorOrder = display.display_ns.enum("ColorMode")
Model = mipi_dbi_ns.enum("Model")

COLOR_ORDERS = {
    MODE_RGB: ColorOrder.COLOR_ORDER_RGB,
    MODE_BGR: ColorOrder.COLOR_ORDER_BGR,
}
DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema

CONF_BUS_MODE = "bus_mode"

# Define models by import from submodule

MODELS = [DriverChip("CUSTOM", {})]
MODELS.extend(amoled.chips)
MODELS.extend(jc.chips)
MODELS.extend(ili.chips)

MODELS = {chip.name: chip for chip in MODELS}


def validate_dimension(value):
    value = cv.positive_int(value)
    if value % 2 != 0:
        raise cv.Invalid("Width/height/offset must be divisible by 2")
    return value


def map_sequence(value):
    """
    The format is a repeated sequence of [CMD, <data>] where <data> is s a sequence of bytes. The length is inferred
    from the length of the sequence and should not be explicit.
    A delay can be inserted by specifying "- delay N" where N is in ms
    """
    if isinstance(value, str) and value.lower().startswith("delay "):
        value = value.lower()[6:]
        delay = cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(TimePeriod(milliseconds=1), TimePeriod(milliseconds=255)),
        )(value)
        return [delay, DELAY_FLAG]
    value = cv.Length(min=1, max=254)(value)
    params = value[1:]
    return [value[0], len(params)] + list(params)


def power_of_two(value):
    value = cv.int_range(1, 128)(value)
    if value & (value - 1) != 0:
        raise cv.Invalid("value must be a power of two")
    return value


BASE_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MIPI_DBI),
        cv.Required(CONF_DIMENSIONS): cv.Any(
            cv.dimensions,
            cv.Schema(
                {
                    cv.Required(CONF_WIDTH): validate_dimension,
                    cv.Required(CONF_HEIGHT): validate_dimension,
                    cv.Optional(CONF_OFFSET_HEIGHT, default=0): validate_dimension,
                    cv.Optional(CONF_OFFSET_WIDTH, default=0): validate_dimension,
                }
            ),
        ),
        cv.Optional(CONF_DRAW_FROM_ORIGIN, default=False): cv.boolean,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DC_PIN): pins.gpio_output_pin_schema,
    }
)


def model_schema(bus_mode, model: DriverChip):
    transform = cv.Schema(
        {
            cv.Optional(CONF_MIRROR_X, False): cv.boolean,
            cv.Optional(CONF_MIRROR_Y, False): cv.boolean,
            cv.Optional(CONF_SWAP_XY, False): cv.boolean,
        }
    )
    # If the model does not support swapping, overwrite the above option
    if model.get_default(CONF_SWAP_XY, False) == cv.UNDEFINED:
        transform = transform.extend(
            {
                cv.Optional(CONF_SWAP_XY): cv.invalid(
                    "Axis swapping not supported by this model"
                )
            }
        )

    # CUSTOM model will need to provide a custom init sequence
    iseqconf = (
        cv.Optional(CONF_INIT_SEQUENCE)
        if model.initsequence
        else cv.Required(CONF_INIT_SEQUENCE)
    )
    schema = BASE_SCHEMA.extend(
        spi.spi_device_schema(
            cs_pin_required=False,
            default_mode="MODE0",
            default_data_rate=10e6,
            mode=bus_mode,
        )
    ).extend(
        {
            model.option(CONF_INVERT_COLORS, False): cv.boolean,
            model.option(CONF_COLOR_ORDER, MODE_BGR): cv.enum(COLOR_ORDERS, upper=True),
            model.option(CONF_DRAW_ROUNDING, 2): power_of_two,
            cv.Optional(CONF_TRANSFORM): transform,
            cv.Optional(CONF_BUS_MODE, default=bus_mode): cv.one_of(
                *model.modes, lower=True
            ),
            cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
            iseqconf: cv.ensure_list(map_sequence),
        }
    )
    if brightness := model.get_default(CONF_BRIGHTNESS):
        schema = schema.extend(
            {
                cv.Optional(CONF_BRIGHTNESS, default=brightness): cv.int_range(
                    0, 0xFF, min_included=True, max_included=True
                ),
            }
        )
    if bus_mode == TYPE_QUAD:
        return cv.All(schema, cv.only_with_esp_idf)
    return schema


def config_schema(config):
    # First get the model and bus mode
    config = cv.Schema(
        {
            cv.Optional(CONF_BUS_MODE): cv.one_of(
                TYPE_SINGLE, TYPE_QUAD, TYPE_OCTAL, lower=True
            ),
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=ALLOW_EXTRA,
    )(config)
    model = MODELS[config[CONF_MODEL]]
    bus_mode = config.get(CONF_BUS_MODE, model.modes[0])
    config = model_schema(bus_mode, model)(config)
    if bus_mode == TYPE_QUAD and CONF_DC_PIN in config:
        raise cv.Invalid("DC pin is not supported in quad mode")
    if bus_mode != TYPE_QUAD and CONF_DC_PIN not in config:
        raise cv.Invalid(f"DC pin is required in {bus_mode} mode")
    return config


CONFIG_SCHEMA = config_schema


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    chip = MODELS[config[CONF_MODEL]]
    if sequence := chip.get_sequence():
        cg.add(var.add_init_sequence(sequence))
    if init_sequences := config.get(CONF_INIT_SEQUENCE):
        sequence = []
        for seq in init_sequences:
            sequence.extend(seq)
        cg.add(var.add_init_sequence(sequence))

    cg.add(var.set_color_order(config[CONF_COLOR_ORDER]))
    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    if CONF_BRIGHTNESS in config:
        cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_draw_from_origin(config[CONF_DRAW_FROM_ORIGIN]))
    cg.add(var.set_draw_rounding(config[CONF_DRAW_ROUNDING]))
    if enable_pin := config.get(CONF_ENABLE_PIN):
        enable = await cg.gpio_pin_expression(enable_pin)
        cg.add(var.set_enable_pin(enable))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if dc_pin := config.get(CONF_DC_PIN):
        dc_pin = await cg.gpio_pin_expression(dc_pin)
        cg.add(var.set_dc_pin(dc_pin))

    transform = config.get(
        CONF_TRANSFORM,
        {
            CONF_MIRROR_X: chip.get_default(CONF_MIRROR_X, False),
            CONF_MIRROR_Y: chip.get_default(CONF_MIRROR_Y, False),
            CONF_SWAP_XY: chip.get_default(CONF_SWAP_XY, False),
        },
    )
    if transform[CONF_MIRROR_X]:
        cg.add(var.set_mirror_x(True))
    if transform[CONF_MIRROR_Y]:
        cg.add(var.set_mirror_y(True))
    if transform.get(CONF_SWAP_XY) is True:
        cg.add(var.set_swap_xy(True))

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

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
