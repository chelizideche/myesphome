import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.cpp_types import PollingComponent

from .. import CONF_LORA_ID, LoRa, LoRaListener, lora_ns

LoRaTransport = lora_ns.class_(
    "LoRaTransport", PacketTransport, PollingComponent, LoRaListener
)

CONFIG_SCHEMA = transport_schema(LoRaTransport).extend(
    {
        cv.GenerateID(CONF_LORA_ID): cv.use_id(LoRa),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    lora = await cg.get_variable(config[CONF_LORA_ID])
    cg.add(var.set_parent(lora))
    cg.add(lora.register_listener(var))
