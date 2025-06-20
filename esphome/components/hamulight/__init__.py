import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, CONF_ADDRESS       # Konstanten für Konfigurationsschlüssel
from esphome.components import output

HAMULIGHT_NAMESPACE = cg.esphome_ns.namespace('hamulight')
HamulightComponent = HAMULIGHT_NAMESPACE.class_('Hamulight', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HamulightComponent),                                      # unique instance ID
    cv.Required("rf_transmit_pin"): output.gpio_output_pin_schema(),                         # required PIN for RF transmission
    cv.Optional("led_pin"): output.gpio_output_pin_schema(),                                 # optional PIN for feedback LED
    cv.Required(CONF_ADDRESS): cv.hex_uint16,                                                # required 2-Byte RF address (HEX)
}).extend(cv.COMPONENT_SCHEMA)

# Function to generate the C++ code from the YAML configuration file
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])                                                  # Creates a new C++ instance or Hamulight class
    await cg.register_component(var, config)                                                 # Register component with ESPHome

    # Create the C++ Code for the PIN and set it for the component
    rf_transmit_pin_code = await cg.gpio_pin_expression(config["rf_transmit_pin"])
    cg.add(var.set_rf_transmit_pin(rf_transmit_pin_code))

    # Create the C++ Code for the LED-PIN (if defined) and set it for the component
    if "led_pin" in config:
        led_pin_code = await cg.gpio_pin_expression(config["led_pin"])
        cg.add(var.set_led_pin(led_pin_code))

    # set the RF address
    cg.add(var.set_rf_address(config[CONF_ADDRESS]))
