import esphome.codegen as cg
from esphome.components import http_request, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["json"]
CODEOWNERS = ["@FredM67"]
DEPENDENCIES = ["uart", "network", "http_request"]

emontx_ns = cg.esphome_ns.namespace("emontx")
EmonTx = emontx_ns.class_("EmonTx", cg.PollingComponent, uart.UARTDevice)

CONF_EMONTX_ID = "emontx_id"
CONF_TAG_NAME = "tag_name"

CONF_EMONCMS = "emoncms"
CONF_SERVER = "server"
CONF_NODE = "node"
CONF_APIKEY = "apikey"
CONF_HTTP_ID = "http_id"  # ID for the HTTP request component

EMONTX_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)

# Schema for the EmonCMS configuration block
EMONCMS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SERVER): cv.url,
        cv.Required(CONF_NODE): cv.string,
        cv.Required(CONF_APIKEY): cv.string,
        cv.Required(CONF_HTTP_ID): cv.use_id(http_request.HttpRequestComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmonTx),
            cv.Optional(CONF_EMONCMS): EMONCMS_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "emontx",
    baud_rate=115200,
    require_tx=False,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Need to add include for http_request component
    cg.add_library("esphome/components/http_request", None)

    # Set EmonCMS configuration if provided
    if CONF_EMONCMS in config:
        emoncms_config = config[CONF_EMONCMS]
        http_var = await cg.get_variable(emoncms_config[CONF_HTTP_ID])
        cg.add(
            var.set_emoncms_config(
                emoncms_config[CONF_SERVER],
                emoncms_config[CONF_NODE],
                emoncms_config[CONF_APIKEY],
                http_var,
            )
        )
