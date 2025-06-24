import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MQTT

AUTO_LOAD = ["json"]
CODEOWNERS = ["@FredM67", "@TrystanLea", "@glynhudson"]

emontx_ns = cg.esphome_ns.namespace("emontx")
EmonTx = emontx_ns.class_("EmonTx", cg.PollingComponent, uart.UARTDevice)

CONF_EMONTX_ID = "emontx_id"
CONF_TAG_NAME = "tag_name"

# EmonCMS config
CONF_EMONCMS = "emoncms"
CONF_HTTP = "http"
CONF_SERVER = "server"
CONF_NODE = "node"
CONF_APIKEY = "apikey"
CONF_BASE_PREFIX = "base_prefix"

EMONTX_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


# Create a reusable function to validate non-empty strings
def not_empty(name):
    def validator(value):
        value = cv.string(value)
        if not value:
            raise cv.Invalid(f"{name} cannot be empty")
        return value

    return validator


# Simple yet effective server URL validation
def validate_server_url(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Server URL cannot be empty")

    # Add https:// prefix if no protocol specified
    if not value.startswith(("http://", "https://")):
        value = "https://" + value

    # Let ESPHome's URL validator handle the rest
    try:
        return cv.url(value)
    except Exception as exc:  # Capture the exception as a variable
        raise cv.Invalid("Please enter a valid server URL") from exc


# For the MQTT schema, we need to handle None values
def empty_mqtt_schema(value):
    if value is None:
        # Return empty dict if mqtt: has no values
        return {}
    # Otherwise validate as normal
    return cv.Schema(
        {
            cv.Optional(CONF_BASE_PREFIX, default="emon"): cv.string,
            cv.Optional(CONF_NODE, default=""): cv.string,
        }
    )(value)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmonTx),
            # Make EmonCMS optional with nested HTTP and MQTT
            cv.Optional(CONF_EMONCMS): cv.Schema(
                {
                    # HTTP config becomes optional within EmonCMS
                    cv.Optional(CONF_HTTP): cv.Schema(
                        {
                            cv.Required(CONF_SERVER): validate_server_url,
                            cv.Required(CONF_NODE): not_empty("Node name"),
                            cv.Required(CONF_APIKEY): not_empty("API key"),
                        }
                    ),
                    # MQTT config becomes optional within EmonCMS
                    cv.Optional(CONF_MQTT): empty_mqtt_schema,
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


# Validate the EmonCMS configuration with both HTTP and MQTT
def validate_emoncms(config):
    if CONF_EMONCMS not in config:
        return config

    emoncms_config = config[CONF_EMONCMS]

    # Validate HTTP part if present
    if CONF_HTTP in emoncms_config:
        from esphome.components import http_request
        from esphome.components.http_request import CONF_HTTP_REQUEST_ID

        cg.add_define("USE_HTTP_REQUEST")

        # Make sure http_request component is defined in YAML
        config = cv.requires_component("http_request")(config)

        # Add HTTP request ID to the HTTP config
        emoncms_config[CONF_HTTP] = cv.Schema(
            {
                cv.Required(CONF_SERVER): validate_server_url,
                cv.Required(CONF_NODE): not_empty("Node name"),
                cv.Required(CONF_APIKEY): not_empty("API key"),
                cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(
                    http_request.HttpRequestComponent
                ),
            }
        )(emoncms_config[CONF_HTTP])

    # Validate MQTT part if present
    if CONF_MQTT in emoncms_config:
        cg.add_define("USE_MQTT_FORWARD")

        mqtt_config = emoncms_config[CONF_MQTT]

        # Ensure we have the default base_prefix
        if CONF_BASE_PREFIX not in mqtt_config:
            mqtt_config[CONF_BASE_PREFIX] = "emon"

        # Set default topic_prefix to device name if not provided or empty
        if CONF_NODE not in mqtt_config or not mqtt_config[CONF_NODE]:
            from esphome.core import CORE

            mqtt_config[CONF_NODE] = CORE.name

            # Add MQTT component as a dependency
            config = cv.requires_component("mqtt")(config)

    return config


# Apply conditional schema
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate_emoncms)

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

    # Set EmonCMS configuration if provided
    if CONF_EMONCMS in config:
        emoncms_config = config[CONF_EMONCMS]

        # Handle HTTP configuration if present
        if CONF_HTTP in emoncms_config:
            from esphome.components.http_request import CONF_HTTP_REQUEST_ID

            http_config = emoncms_config[CONF_HTTP]

            http_var = await cg.get_variable(http_config[CONF_HTTP_REQUEST_ID])

            cg.add(
                var.set_http_forward(
                    http_config[CONF_SERVER],
                    http_config[CONF_NODE],
                    http_config[CONF_APIKEY],
                    http_var,
                )
            )

        # Handle MQTT configuration if present
        if CONF_MQTT in emoncms_config:
            mqtt_config = emoncms_config[CONF_MQTT]

            base_prefix = mqtt_config[CONF_BASE_PREFIX]
            topic_prefix = mqtt_config[CONF_NODE]  # Use NODE as topic_prefix

            # Call updated method with both prefixes
            cg.add(var.set_mqtt_forward(base_prefix, topic_prefix))
