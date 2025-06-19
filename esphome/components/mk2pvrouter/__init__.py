import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MQTT, CONF_TOPIC_PREFIX

CODEOWNERS = ["@FredM67"]

mk2pvrouter_ns = cg.esphome_ns.namespace("mk2pvrouter")
Mk2PVRouter = mk2pvrouter_ns.class_("Mk2PVRouter", cg.PollingComponent, uart.UARTDevice)

CONF_MK2PVROUTER_ID = "mk2pvrouter_id"
CONF_TAG_NAME = "tag_name"

MK2PVROUTER_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MK2PVROUTER_ID): cv.use_id(Mk2PVRouter),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Mk2PVRouter),
            # Make MQTT forwarding optional
            cv.Optional(CONF_MQTT): cv.Any(dict),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


# Create a reusable function to validate non-empty strings
def not_empty(name):
    def validator(value):
        value = cv.string(value)
        if not value:
            raise cv.Invalid(f"{name} cannot be empty")
        return value

    return validator


# Validate MQTT forward config
def validate_mqtt_forward(config):
    # Skip if no MQTT forwarding configuration
    if CONF_MQTT in config:
        cg.add_define("USE_MQTT_FORWARD")

        # Validate MQTT forwarding configuration
        mqtt_schema = cv.Schema(
            {
                cv.Required(CONF_TOPIC_PREFIX): not_empty("Topic prefix"),
            }
        )
        config[CONF_MQTT] = mqtt_schema(config[CONF_MQTT])
    return config


# Apply conditional schema
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate_mqtt_forward)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Set MQTT forwarding if configured
    if CONF_MQTT in config:
        mqtt_config = config[CONF_MQTT]
        cg.add(var.set_mqtt_forward(mqtt_config[CONF_TOPIC_PREFIX]))
