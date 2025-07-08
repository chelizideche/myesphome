import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins
from esphome.components import number, sensor

CODEOWNERS = ["@madmat17"]

# C++ namespace and component
hamulight_ns = cg.esphome_ns.namespace("hamulight")
HamulightComponent = hamulight_ns.class_("HamulightComponent", cg.Component)

CONF_RF_TRANSMIT_PIN = "rf_transmit_pin"
CONF_RF_ADDRESS = "rf_address"
CONF_LED_PIN = "led_pin"

# Command scanner option keys (nested)
CONF_COMMAND_SCANNER = "command_scanner"
CONF_ENABLED = "enabled"
CONF_CMDSCAN_START = "cmdscan_start"
CONF_CMDSCAN_END = "cmdscan_end"
CONF_CMDSCAN_PAUSE = "cmdscan_pause"
CONF_LAST_SCANNED_SENSOR = "last_scanned_sensor"

# Command scanner config schema (optional)
COMMAND_SCANNER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_CMDSCAN_START): cv.use_id(number.Number),
        cv.Optional(CONF_CMDSCAN_END): cv.use_id(number.Number),
        cv.Optional(CONF_CMDSCAN_PAUSE): cv.use_id(number.Number),
        cv.Optional(CONF_LAST_SCANNED_SENSOR): cv.use_id(sensor.Sensor),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HamulightComponent),
        cv.Required(CONF_RF_TRANSMIT_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_RF_ADDRESS): cv.hex_uint16_t,
        cv.Optional(CONF_LED_PIN): pins.gpio_output_pin_schema,
        # Optional nested command scanner block
        cv.Optional(CONF_COMMAND_SCANNER): COMMAND_SCANNER_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set pin numbers for logging and hardware
    rf_pin_num = config[CONF_RF_TRANSMIT_PIN]["number"]
    cg.add(var.set_rf_pin_num(rf_pin_num))
    rf_transmit_pin_code = await cg.gpio_pin_expression(config[CONF_RF_TRANSMIT_PIN])
    cg.add(var.set_rf_transmit_pin(rf_transmit_pin_code))
    cg.add(var.set_rf_address(config[CONF_RF_ADDRESS]))

    # Optional feedback LED
    if CONF_LED_PIN in config:
        led_pin_code = await cg.gpio_pin_expression(config[CONF_LED_PIN])
        cg.add(var.set_led_pin(led_pin_code))
        cg.add(var.set_led_pin_num(config[CONF_LED_PIN]["number"]))

    # Handle command_scanner block if present
    command_scanner = config.get(CONF_COMMAND_SCANNER)
    if command_scanner is not None and command_scanner.get(CONF_ENABLED, True):
        # Link optional command scanner numbers and sensor
        if CONF_CMDSCAN_START in command_scanner:
            num = await cg.get_variable(command_scanner[CONF_CMDSCAN_START])
            cg.add(var.set_cmdscan_start_number(num))
        if CONF_CMDSCAN_END in command_scanner:
            num = await cg.get_variable(command_scanner[CONF_CMDSCAN_END])
            cg.add(var.set_cmdscan_end_number(num))
        if CONF_CMDSCAN_PAUSE in command_scanner:
            num = await cg.get_variable(command_scanner[CONF_CMDSCAN_PAUSE])
            cg.add(var.set_cmdscan_pause_number(num))
        if CONF_LAST_SCANNED_SENSOR in command_scanner:
            s = await cg.get_variable(command_scanner[CONF_LAST_SCANNED_SENSOR])
            cg.add(var.set_last_scanned_sensor(s))
    # If command_scanner missing or disabled, do not wire up scanner functionality
