from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32_rmt, remote_base
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_DUMP,
    CONF_FILTER,
    CONF_ID,
    CONF_IDLE,
    CONF_MAX_LENGTH,
    CONF_MEMORY_BLOCKS,
    CONF_MIN_LENGTH,
    CONF_PIN,
    CONF_RMT_CHANNEL,
    CONF_TOLERANCE,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE, TimePeriod

CONF_CLOCK_DIVIDER = "clock_divider"
CONF_CLOCK_RESOLUTION = "clock_resolution"
CONF_WITH_DMA = "with_dma"

AUTO_LOAD = ["remote_base"]
remote_receiver_ns = cg.esphome_ns.namespace("remote_receiver")
remote_base_ns = cg.esphome_ns.namespace("remote_base")

ToleranceMode = remote_base_ns.enum("ToleranceMode")

TYPE_PERCENTAGE = "percentage"
TYPE_TIME = "time"

TOLERANCE_MODE = {
    TYPE_PERCENTAGE: ToleranceMode.TOLERANCE_MODE_PERCENTAGE,
    TYPE_TIME: ToleranceMode.TOLERANCE_MODE_TIME,
}

TOLERANCE_SCHEMA = cv.typed_schema(
    {
        TYPE_PERCENTAGE: cv.Schema(
            {cv.Required(CONF_VALUE): cv.All(cv.percentage_int, cv.uint32_t)}
        ),
        TYPE_TIME: cv.Schema(
            {
                cv.Required(CONF_VALUE): cv.All(
                    cv.positive_time_period_microseconds,
                    cv.Range(max=TimePeriod(microseconds=4294967295)),
                )
            }
        ),
    },
    lower=True,
    enum=TOLERANCE_MODE,
)

RemoteReceiverComponent = remote_receiver_ns.class_(
    "RemoteReceiverComponent", remote_base.RemoteReceiverBase, cg.Component
)


def validate_config(config):
    if CORE.is_esp32:
        if esp32_rmt.new_rmt_driver():
            if CONF_CLOCK_DIVIDER in config:
                raise cv.Invalid(
                    "clock_divider has been deprecated in this version of the RMT driver, please use clock_resolution"
                )
        elif CONF_CLOCK_RESOLUTION in config:
            raise cv.Invalid(
                "clock_resolution is not available in this version of the RMT driver, please use clock_divider"
            )


def validate_tolerance(value):
    if isinstance(value, dict):
        return TOLERANCE_SCHEMA(value)

    if "%" in str(value):
        type_ = TYPE_PERCENTAGE
    else:
        try:
            cv.positive_time_period_microseconds(value)
            type_ = TYPE_TIME
        except cv.Invalid as exc:
            raise cv.Invalid(
                "Tolerance must be a percentage or time. Configurations made before 2024.5.0 treated the value as a percentage."
            ) from exc

    return TOLERANCE_SCHEMA(
        {
            CONF_VALUE: value,
            CONF_TYPE: type_,
        }
    )


MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = validate_config
CONFIG_SCHEMA = remote_base.validate_triggers(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RemoteReceiverComponent),
            cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
            cv.Optional(CONF_TOLERANCE, default="25%"): validate_tolerance,
            cv.SplitDefault(
                CONF_BUFFER_SIZE,
                esp32="10000b",
                esp8266="1000b",
                bk72xx="1000b",
                rtl87xx="1000b",
            ): cv.validate_bytes,
            cv.Optional(CONF_FILTER, default="50us"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.Optional(CONF_CLOCK_RESOLUTION): cv.All(
                cv.only_on_esp32, cv.Range(min=1, max=80000000)
            ),
            cv.Optional(CONF_CLOCK_DIVIDER): cv.All(
                cv.only_on_esp32, cv.Range(min=1, max=255)
            ),
            cv.Optional(CONF_IDLE, default="10ms"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.Optional(CONF_MIN_LENGTH, default=0): cv.Range(min=0),
            cv.Optional(CONF_MAX_LENGTH, default=0): cv.Range(min=0),
            cv.Optional(CONF_WITH_DMA, default=False): cv.boolean,
            cv.Optional(CONF_MEMORY_BLOCKS, default=3): cv.Range(min=1, max=8),
            cv.Optional(CONF_RMT_CHANNEL): esp32_rmt.validate_rmt_channel(tx=False),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    if CORE.is_esp32:
        new_driver = esp32_rmt.new_rmt_driver()
        rmt_channel = config.get(CONF_RMT_CHANNEL, None)
        if not new_driver and rmt_channel is not None:
            var = cg.new_Pvariable(
                config[CONF_ID], pin, rmt_channel, config[CONF_MEMORY_BLOCKS]
            )
        else:
            var = cg.new_Pvariable(config[CONF_ID], pin, config[CONF_MEMORY_BLOCKS])
        if CONF_CLOCK_DIVIDER in config:
            cg.add(var.set_clock_divider(config[CONF_CLOCK_DIVIDER]))
        if CONF_CLOCK_RESOLUTION in config:
            cg.add(var.set_clock_resolution(config[CONF_CLOCK_RESOLUTION]))
        if new_driver:
            cg.add(var.set_min_length(config[CONF_MIN_LENGTH]))
            cg.add(var.set_max_length(config[CONF_MAX_LENGTH]))
            cg.add(var.set_with_dma(config[CONF_WITH_DMA]))
            cg.add_define("USE_NEW_RMT_DRIVER")
    else:
        var = cg.new_Pvariable(config[CONF_ID], pin)

    dumpers = await remote_base.build_dumpers(config[CONF_DUMP])
    for dumper in dumpers:
        cg.add(var.register_dumper(dumper))

    triggers = await remote_base.build_triggers(config)
    for trigger in triggers:
        cg.add(var.register_listener(trigger))
    await cg.register_component(var, config)

    cg.add(
        var.set_tolerance(
            config[CONF_TOLERANCE][CONF_VALUE], config[CONF_TOLERANCE][CONF_TYPE]
        )
    )
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_idle_us(config[CONF_IDLE]))
