import esphome.codegen as cg
from esphome.components.lora import LoRa, lora_schema, new_lora
import esphome.config_validation as cv
from esphome.cpp_types import Component

from .. import CONF_SX127X_ID, SX127x, SX127xListener, sx127x_modes, sx127x_ns

SX127xLoRa = sx127x_ns.class_("SX127xLoRa", LoRa, Component, SX127xListener)


def validate_lora(config):
    radio_id = str(config[CONF_SX127X_ID])
    mode = sx127x_modes.get(radio_id)
    if mode is not None and mode != "LORA":
        raise cv.Invalid(
            f"SX127x comonent {radio_id} is not configured in modulation LORA"
        )
    return config


CONFIG_SCHEMA = (
    lora_schema(SX127xLoRa)
    .extend(
        {
            cv.GenerateID(CONF_SX127X_ID): cv.use_id(SX127x),
        }
    )
    .add_extra(validate_lora)
)


async def to_code(config):
    # Call the validate_lora function here as well, config validation is done in the
    # order as defined in config.yaml, so if lora is configured before sx127x, the
    # sx127x mode will not be set yet.
    validate_lora(config)

    var = await new_lora(config)
    sx127x = await cg.get_variable(config[CONF_SX127X_ID])
    cg.add(var.set_parent(sx127x))
    cg.add(sx127x.register_listener(var))
