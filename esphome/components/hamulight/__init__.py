import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins

HAMULIGHT_NAMESPACE = cg.esphome_ns.namespace('hamulight')
HamulightComponent = HAMULIGHT_NAMESPACE.class_('HamulightComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HamulightComponent),                                               # unique instance ID
    cv.Required("rf_transmit_pin"): pins.gpio_output_pin_schema,                                      # required PIN for RF transmission
    cv.Optional("led_pin"): pins.gpio_output_pin_schema,                                              # optional PIN for feedback LED
    cv.Required("rf_address"): cv.hex_uint16_t,                                                       # required 2-Byte RF address (HEX)
    cv.Optional("command_scanner"): cv.Schema({                                                       # optional command scanner block
        cv.Optional("enabled", default=True): cv.boolean,
        cv.Required(CONF_CMDSCAN_START): cv.use_id(cg.NUMBER_ID),
        cv.Required(CONF_CMDSCAN_END): cv.use_id(cg.NUMBER_ID),
        cv.Required(CONF_CMDSCAN_PAUSE): cv.use_id(cg.NUMBER_ID),
    }),
}).extend(cv.COMPONENT_SCHEMA)

# Function to generate the C++ code from the YAML configuration file
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])                                                          # Creates a new C++ instance of HamulightComponent class
    await cg.register_component(var, config)                                                         # Register component with ESPHome

    # Pass pin number for RMT
    rf_pin_num = config["rf_transmit_pin"]["number"]
    cg.add(var.set_rf_pin_num(rf_pin_num))

    # Pass GPIOPin object for optional digitalWrite use
    rf_transmit_pin_code = await cg.gpio_pin_expression(config["rf_transmit_pin"])
    cg.add(var.set_rf_transmit_pin(rf_transmit_pin_code))

    # Create the C++ Code for the LED-PIN (if defined) and set it for the component
    if "led_pin" in config:
        led_pin_code = await cg.gpio_pin_expression(config["led_pin"])
        cg.add(var.set_led_pin(led_pin_code))

    # Set the RF address
    cg.add(var.set_rf_address(config["rf_address"]))

    # Command Scanner (optional)
    if "command_scanner" in config:
        cs_cfg = config["command_scanner"]
        cg.add(var.set_command_scanner_enabled(cs_cfg.get("enabled", True)))
        start = await cg.get_variable(cs_cfg[CONF_CMDSCAN_START])
        end = await cg.get_variable(cs_cfg[CONF_CMDSCAN_END])
        pause = await cg.get_variable(cs_cfg[CONF_CMDSCAN_PAUSE])
        cg.add(var.set_cmdscan_start_number(start))
        cg.add(var.set_cmdscan_end_number(end))
        cg.add(var.set_cmdscan_pause_number(pause))
    else:
        cg.add(var.set_command_scanner_enabled(False))
