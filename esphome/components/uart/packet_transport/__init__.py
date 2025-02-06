from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.cpp_types import PollingComponent

from .. import UART_DEVICE_SCHEMA, register_uart_device, uart_ns

CODEOWNERS = ["@clydebarrow"]

UDPTransport = uart_ns.class_("UARTTransport", PacketTransport, PollingComponent)

CONFIG_SCHEMA = transport_schema(UDPTransport).extend(UART_DEVICE_SCHEMA)


async def to_code(config):
    var = await new_packet_transport(config)
    await register_uart_device(var, config)
