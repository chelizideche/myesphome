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

from . import (
    CONF_DRAW_FROM_ORIGIN,
    CONF_DRAW_ROUNDING,
    CONF_PIXEL_MODE,
    CONF_SPI_16,
    CONF_USE_MIRROR,
    MODE_BGR,
    MODE_RGB,
)
from .models import (
    DELAY_FLAG,
    MADCTL_BGR,
    MADCTL_MV,
    MADCTL_MX,
    MADCTL_MY,
    MADCTL_XFLIP,
    MADCTL_YFLIP,
    DriverChip,
    amoled,
    ili,
    jc,
)
from .models.commands import BRIGHTNESS, DISPON, INVOFF, INVON, MADCTL, PIXFMT, SLPOUT

DEPENDENCIES = ["spi"]

mipi_spi_ns = cg.esphome_ns.namespace("mipi_spi")
MIPI_SPI = mipi_spi_ns.class_(
    "MipiSpi", display.Display, display.DisplayBuffer, cg.Component, spi.SPIDevice
)
ColorOrder = display.display_ns.enum("ColorMode")
Model = mipi_spi_ns.enum("Model")

COLOR_ORDERS = {
    MODE_RGB: ColorOrder.COLOR_ORDER_RGB,
    MODE_BGR: ColorOrder.COLOR_ORDER_BGR,
}
DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema

CONF_BUS_MODE = "bus_mode"

# Define models by import from submodule

MODELS = [DriverChip("CUSTOM", {})]
MODELS.extend(amoled.models)
MODELS.extend(jc.models)
MODELS.extend(ili.models)

MODELS = {model.name: model for model in MODELS}

PixelMode = mipi_spi_ns.enum("PixelMode")


PIXEL_MODES = {
    "16bit": 0x55,
    "18bit": 0x66,
}


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


DIMENSION_SCHEMA = cv.Any(
    cv.dimensions,
    cv.Schema(
        {
            cv.Required(CONF_WIDTH): validate_dimension,
            cv.Required(CONF_HEIGHT): validate_dimension,
            cv.Optional(CONF_OFFSET_HEIGHT, default=0): validate_dimension,
            cv.Optional(CONF_OFFSET_WIDTH, default=0): validate_dimension,
        }
    ),
)

BASE_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MIPI_SPI),
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
        }
    )
    if model.get_default(CONF_SWAP_XY, False) == cv.UNDEFINED:
        transform = transform.extend(
            {
                cv.Optional(CONF_SWAP_XY): cv.invalid(
                    "Axis swapping not supported by this model"
                )
            }
        )
    else:
        transform = transform.extend(
            {
                cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
            }
        )
    # CUSTOM model will need to provide a custom init sequence
    iseqconf = (
        cv.Required(CONF_INIT_SEQUENCE)
        if model.initsequence is None
        else cv.Optional(CONF_INIT_SEQUENCE)
    )
    schema = (
        BASE_SCHEMA.extend(
            spi.spi_device_schema(
                cs_pin_required=False,
                default_mode="MODE0",
                default_data_rate=10e6,
                mode=bus_mode,
            )
        )
        .extend(
            {
                model.option(CONF_COLOR_ORDER, MODE_BGR): cv.enum(
                    COLOR_ORDERS, upper=True
                ),
                model.option(CONF_DRAW_ROUNDING, 2): power_of_two,
                model.option(CONF_PIXEL_MODE, "16bit"): cv.Any(
                    cv.one_of(*PIXEL_MODES, lower=True),
                    cv.int_range(0, 255, min_included=True, max_included=True),
                ),
                cv.Optional(CONF_TRANSFORM): transform,
                cv.Optional(CONF_BUS_MODE, default=bus_mode): cv.one_of(
                    *model.modes, lower=True
                ),
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                iseqconf: cv.ensure_list(map_sequence),
            }
        )
        .extend(
            {
                model.option(x): cv.boolean
                for x in [
                    CONF_DRAW_FROM_ORIGIN,
                    CONF_SPI_16,
                    CONF_INVERT_COLORS,
                    CONF_USE_MIRROR,
                ]
            }
        )
    )
    if model.get_default(CONF_WIDTH) and model.get_default(CONF_HEIGHT):
        schema = schema.extend(
            {
                cv.Optional(CONF_DIMENSIONS): DIMENSION_SCHEMA,
            }
        )
    else:
        schema = schema.extend(
            {
                cv.Required(CONF_DIMENSIONS): DIMENSION_SCHEMA,
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


def get_transform(model, config):
    return config.get(
        CONF_TRANSFORM,
        {
            CONF_MIRROR_X: model.get_default(CONF_MIRROR_X, False),
            CONF_MIRROR_Y: model.get_default(CONF_MIRROR_Y, False),
            CONF_SWAP_XY: model.get_default(CONF_SWAP_XY, False),
        },
    )


def get_sequence(model, config):
    """
    Create the init sequence for the display.
    Use the default sequence from the model, if any, and append any custom sequence provided in the config.
    Append SLPOUT (if not already in the sequence) and DISPON to the end of the sequence
    Pixel format, color order, and orientation will be set.
    """
    sequence = list(model.initsequence)
    custom_sequence = config.get(CONF_INIT_SEQUENCE, [])
    sequence.extend(custom_sequence)
    # Ensure each command is a tuple
    sequence = [x if isinstance(x, tuple) else (x,) for x in sequence]
    commands = [x[0] for x in custom_sequence]
    # Set pixel format if not already in the custom sequence
    if PIXFMT not in commands:
        pixel_mode = config[CONF_PIXEL_MODE]
        if not isinstance(pixel_mode, int):
            pixel_mode = PIXEL_MODES[pixel_mode]
        sequence.append((PIXFMT, pixel_mode))
    use_mirror = config[CONF_USE_MIRROR]
    if MADCTL not in commands:
        madctl = 0
        transform = get_transform(model, config)
        if transform.get(CONF_MIRROR_X):
            madctl |= MADCTL_XFLIP if use_mirror else MADCTL_MX
        if transform.get(CONF_MIRROR_Y):
            madctl |= MADCTL_YFLIP if use_mirror else MADCTL_MY
        if transform.get(CONF_SWAP_XY) is True:  # Exclude Undefined
            madctl |= MADCTL_MV
        if config[CONF_COLOR_ORDER] == MODE_BGR:
            madctl |= MADCTL_BGR
        sequence.append((MADCTL, madctl))
    if INVON not in commands and INVOFF not in commands:
        if config[CONF_INVERT_COLORS]:
            sequence.append((INVON,))
        else:
            sequence.append((INVOFF,))
    if BRIGHTNESS not in commands:
        if brightness := config.get(
            CONF_BRIGHTNESS, model.get_default(CONF_BRIGHTNESS)
        ):
            sequence.append((BRIGHTNESS, brightness))
    if SLPOUT not in commands:
        sequence.append((SLPOUT,))
    if DISPON not in commands:
        sequence.append((DISPON,))

    # Flatten the sequence into a list of bytes, with the length of each command
    # or the delay flag inserted where needed
    return sum(
        tuple(
            (x[1], 0xFF) if x[0] == DELAY_FLAG else (x[0], len(x) - 1) + x[1:]
            for x in sequence
        ),
        (),
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    model = MODELS[config[CONF_MODEL]]
    cg.add(var.set_init_sequence(get_sequence(model, config)))
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_draw_from_origin(config[CONF_DRAW_FROM_ORIGIN]))
    cg.add(var.set_draw_rounding(config[CONF_DRAW_ROUNDING]))
    cg.add(var.set_spi_16(config[CONF_SPI_16]))
    if enable_pin := config.get(CONF_ENABLE_PIN):
        enable = await cg.gpio_pin_expression(enable_pin)
        cg.add(var.set_enable_pin(enable))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if dc_pin := config.get(CONF_DC_PIN):
        dc_pin = await cg.gpio_pin_expression(dc_pin)
        cg.add(var.set_dc_pin(dc_pin))

    transform = get_transform(model, config)
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
    else:
        width = model.get_default(CONF_WIDTH)
        height = model.get_default(CONF_HEIGHT)
        offset_width = model.get_default(CONF_OFFSET_WIDTH, 0)
        offset_height = model.get_default(CONF_OFFSET_HEIGHT, 0)

        # Swap default dimensions if swap_xy is set (not for explicit dimensions)
        if transform[CONF_SWAP_XY]:
            cg.add(var.set_dimensions(height, width))
            cg.add(var.set_offsets(offset_height, offset_width))
        else:
            cg.add(var.set_dimensions(width, height))
            cg.add(var.set_offsets(offset_width, offset_height))

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
