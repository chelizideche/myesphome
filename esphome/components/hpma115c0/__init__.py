import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = [
    "@Ben0it",
]
DEPENDENCIES = [
    "uart",
]
MULTI_CONF = True

hpma115c0_ns = cg.esphome_ns.namespace("hpma115c0")
Hpma115C0PollingComponent = hpma115c0_ns.class_(
    "Hpma115C0PollingComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_HPMA115C0_ID = "hpma115c0_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Hpma115C0PollingComponent),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s")),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "uart_id",
    require_tx=True,
    require_rx=True,
    baud_rate=9600,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
