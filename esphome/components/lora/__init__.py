"""ESPHome LoRa component."""

import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM

CODEOWNERS = ["@dala318"]

lora_ns = cg.esphome_ns.namespace("lora")
LoRa = lora_ns.class_("LoRa", cg.Component)

IS_PLATFORM_COMPONENT = True

DOMAIN = "lora"

CONF_LORA_ID = "lora_id"

# CONF_BANDWIDTH = "bandwidth"
# CONF_CODING_RATE = "coding_rate"
# CONF_PAYLOAD_LENGTH = "payload_length"
# CONF_PREAMBLE_SIZE = "preamble_size"
# CONF_SPREADING_FACTOR = "spreading_factor"

# BANDWIDTHS = [
#     "7_8kHz",
#     "10_4kHz",
#     "15_6kHz",
#     "20_8kHz",
#     "31_3kHz",
#     "41_7kHz",
#     "62_5kHz",
#     "125_0kHz",
#     "250_0kHz",
#     "500_0kHz",
# ]
# CODING_RATES = [
#     "CR_4_5",
#     "CR_4_6",
#     "CR_4_7",
#     "CR_4_8",
# ]


_LOGGER = logging.getLogger(__name__)


LORA_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LORA_ID): cv.use_id(LoRa),
    }
)

# def validate_(config):
#     if config[CONF_PREAMBLE_SIZE] > 0 and config[CONF_PREAMBLE_SIZE] < 6:
#         raise cv.Invalid("Minimum preamble size is 6 with LORA")
#     if config[CONF_SPREADING_FACTOR] == 6 and config[CONF_PAYLOAD_LENGTH] == 0:
#         raise cv.Invalid("Payload length must be set when spreading factor is 6")
#     return config


LORA_COMPONENT_SCHEMA = (
    cv.Schema(
        {
            # cv.Optional(CONF_BANDWIDTH, default="125_0kHz"): cv.one_of(BANDWIDTHS),
            # cv.Optional(CONF_CODING_RATE, default="CR_4_5"): cv.one_of(CODING_RATES),
            # cv.Optional(CONF_PAYLOAD_LENGTH, default=0): cv.int_range(min=0, max=256),
            # cv.Optional(CONF_PREAMBLE_SIZE, default=0): cv.int_range(min=0, max=65535),
            # cv.Optional(CONF_SPREADING_FACTOR, default=7): cv.int_range(min=6, max=12),
        }
    ).extend(cv.COMPONENT_SCHEMA)
    # .add_extra(validate_)
)


async def register_lora_client(var, config):
    lora_var = await cg.get_variable(config[CONF_LORA_ID])
    cg.add(var.set_parent(lora_var))
    return lora_var


def lora_schema(cls):
    return LORA_COMPONENT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(cls)})


async def new_lora(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_platform_name(config[CONF_PLATFORM]))
    await cg.register_component(var, config)
    return var
