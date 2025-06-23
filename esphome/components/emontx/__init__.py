import logging

import esphome.codegen as cg
from esphome.components import mqtt, uart
import esphome.config_validation as cv
from esphome.const import CONF_DISCOVERY, CONF_ID, CONF_MQTT, CONF_TOPIC_PREFIX

AUTO_LOAD = ["json"]
CODEOWNERS = ["@FredM67", "@TrystanLea", "@glynhudson"]
MULTI_CONF = True

emontx_ns = cg.esphome_ns.namespace("emontx")
EmonTx = emontx_ns.class_("EmonTx", cg.PollingComponent, uart.UARTDevice)

CONF_EMONTX_ID = "emontx_id"
CONF_TAG_NAME = "tag_name"

# EmonCMS config
CONF_EMONCMS = "emoncms"
CONF_SERVER = "server"
CONF_NODE = "node"
CONF_APIKEY = "apikey"

EMONTX_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


# Define this at module level to access from other functions
class EmonTxState:
    mqtt_config = None


emontx_state = EmonTxState()


# First pass - capture EmonTX MQTT config for later use
def capture_mqtt_config(config):
    if CONF_MQTT in config:
        emontx_state.mqtt_config = config[CONF_MQTT]
        logging.info("EmonTX MQTT config captured for injection")
    return config


# Create a reusable function to validate non-empty strings
def not_empty(name):
    def validator(value):
        value = cv.string(value)
        if not value:
            raise cv.Invalid(f"{name} cannot be empty")
        return value

    return validator


# Store original schema
if hasattr(mqtt, "CONFIG_SCHEMA"):
    original_mqtt_schema = mqtt.CONFIG_SCHEMA

    # Create a patched schema that injects our settings
    def patched_mqtt_schema(config):
        # Inject EmonTX MQTT settings if available
        if emontx_state.mqtt_config:
            # Transfer topic_prefix
            if CONF_TOPIC_PREFIX in emontx_state.mqtt_config:
                topic_prefix = emontx_state.mqtt_config[CONF_TOPIC_PREFIX]
                logging.info(f"Injecting topic_prefix from EmonTX: {topic_prefix}")
                config[CONF_TOPIC_PREFIX] = topic_prefix

            # Transfer discovery setting
            if CONF_DISCOVERY in emontx_state.mqtt_config:
                discovery = emontx_state.mqtt_config[CONF_DISCOVERY]
                logging.info(f"Injecting discovery from EmonTX: {discovery}")
                config[CONF_DISCOVERY] = discovery

        # Apply original schema
        return original_mqtt_schema(config)

    # Replace MQTT's schema with our patched version
    mqtt.CONFIG_SCHEMA = patched_mqtt_schema
    logging.info("MQTT schema successfully patched")


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


# Conditionally add EmonCMS schema
def validate_emoncms(config):
    # Skip if no EmonCMS configuration
    if CONF_EMONCMS in config:
        from esphome.components import http_request
        from esphome.components.http_request import CONF_HTTP_REQUEST_ID

        cg.add_define("USE_HTTP_REQUEST")

        # Make sure http_request component is defined in YAML
        config = cv.requires_component("http_request")(config)

        # Validate EmonCMS configuration
        emoncms_schema = cv.Schema(
            {
                cv.Required(CONF_SERVER): validate_server_url,
                cv.Required(CONF_NODE): not_empty("Node name"),
                cv.Required(CONF_APIKEY): not_empty("API key"),
                cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(
                    http_request.HttpRequestComponent
                ),
            }
        )
        config[CONF_EMONCMS] = emoncms_schema(config[CONF_EMONCMS])
    return config


# Validate MQTT forward config
def validate_mqtt_forward(config):
    # Skip if no MQTT forwarding configuration
    if CONF_MQTT in config:
        cg.add_define("USE_MQTT_FORWARD")

        # Validate MQTT forwarding configuration
        mqtt_schema = cv.Schema(
            {
                cv.Required(CONF_TOPIC_PREFIX): not_empty("Topic prefix"),
                cv.Optional(CONF_DISCOVERY, default=False): cv.boolean,
            }
        )
        config[CONF_MQTT] = mqtt_schema(config[CONF_MQTT])

        # Add MQTT component as a dependency
        # This checks if mqtt component exists in the configuration
        config = cv.requires_component("mqtt")(config)

    return config


CONFIG_SCHEMA = cv.All(
    capture_mqtt_config,  # Capture MQTT config first
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmonTx),
            # Make MQTT forwarding optional
            cv.Optional(CONF_MQTT): cv.Schema(
                {
                    cv.Required(CONF_TOPIC_PREFIX): not_empty("Topic prefix"),
                    cv.Optional(CONF_DISCOVERY, default=False): cv.boolean,
                }
            ),
            # Make EmonCMS optional
            cv.Optional(CONF_EMONCMS): cv.Any(dict),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    validate_emoncms,
    validate_mqtt_forward,
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

    # Set MQTT forwarding if configured
    if CONF_MQTT in config:
        mqtt_config = config[CONF_MQTT]
        topic_prefix = mqtt_config[CONF_TOPIC_PREFIX]

        # Log what's being used
        logging.info(f"EmonTX using topic_prefix: {topic_prefix}")
        if CONF_DISCOVERY in mqtt_config:
            logging.info(f"EmonTX using discovery: {mqtt_config[CONF_DISCOVERY]}")

        cg.add(var.set_mqtt_forward(topic_prefix))

    # Set EmonCMS configuration if provided
    if CONF_EMONCMS in config:
        from esphome.components.http_request import CONF_HTTP_REQUEST_ID

        emoncms_config = config[CONF_EMONCMS]

        http_var = await cg.get_variable(emoncms_config[CONF_HTTP_REQUEST_ID])

        cg.add(
            var.set_emoncms_config(
                emoncms_config[CONF_SERVER],
                emoncms_config[CONF_NODE],
                emoncms_config[CONF_APIKEY],
                http_var,
            )
        )
