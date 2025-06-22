import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins
from esphome.components import button, number, sensor

HAMULIGHT_NAMESPACE = cg.esphome_ns.namespace('hamulight')
HamulightComponent = HAMULIGHT_NAMESPACE.class_('HamulightComponent', cg.Component)

CONF_RF_TRANSMIT_PIN = "rf_transmit_pin"
CONF_RF_ADDRESS = "rf_address"
CONF_LED_PIN = "led_pin"

# Button, number, and sensor IDs to link
CONF_TOGGLE_BTN = "toggle_btn"
CONF_PAIR_BTN = "pair_btn"
CONF_CMDSCAN_BTN = "cmdscan_btn"
CONF_CMDSCAN_STOP_BTN = "cmdscan_stop_btn"

CONF_BRIGHTNESS_NUMBER = "brightness_number"
CONF_CMDSCAN_START = "cmdscan_start"
CONF_CMDSCAN_END = "cmdscan_end"
CONF_CMDSCAN_PAUSE = "cmdscan_pause"

CONF_LAST_SCANNED_SENSOR = "last_scanned_sensor"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HamulightComponent),
    cv.Required(CONF_RF_TRANSMIT_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_LED_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_RF_ADDRESS): cv.hex_uint16_t,

    # Optional entity references, user must provide id: of template entity in YAML
    cv.Optional(CONF_TOGGLE_BTN): cv.use_id(button.Button),
    cv.Optional(CONF_PAIR_BTN): cv.use_id(button.Button),
    cv.Optional(CONF_CMDSCAN_BTN): cv.use_id(button.Button),
    cv.Optional(CONF_CMDSCAN_STOP_BTN): cv.use_id(button.Button),

    cv.Optional(CONF_BRIGHTNESS_NUMBER): cv.use_id(number.Number),
    cv.Optional(CONF_CMDSCAN_START): cv.use_id(number.Number),
    cv.Optional(CONF_CMDSCAN_END): cv.use_id(number.Number),
    cv.Optional(CONF_CMDSCAN_PAUSE): cv.use_id(number.Number),

    cv.Optional(CONF_LAST_SCANNED_SENSOR): cv.use_id(sensor.Sensor),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    rf_pin_num = config[CONF_RF_TRANSMIT_PIN]["number"]
    cg.add(var.set_rf_pin_num(rf_pin_num))
    rf_transmit_pin_code = await cg.gpio_pin_expression(config[CONF_RF_TRANSMIT_PIN])
    cg.add(var.set_rf_transmit_pin(rf_transmit_pin_code))
    if CONF_LED_PIN in config:
        led_pin_code = await cg.gpio_pin_expression(config[CONF_LED_PIN])
        cg.add(var.set_led_pin(led_pin_code))
        cg.add(var.set_led_pin_num(config[CONF_LED_PIN]["number"]))
    cg.add(var.set_rf_address(config[CONF_RF_ADDRESS]))

    # Link buttons to actions
    if CONF_TOGGLE_BTN in config:
        btn = await cg.get_variable(config[CONF_TOGGLE_BTN])
        cg.add(btn.add_on_press(var.toggle))
    if CONF_PAIR_BTN in config:
        btn = await cg.get_variable(config[CONF_PAIR_BTN])
        cg.add(btn.add_on_press(var.pair_with_driver))
    if CONF_CMDSCAN_BTN in config:
        btn = await cg.get_variable(config[CONF_CMDSCAN_BTN])
        cg.add(btn.add_on_press(var.start_command_scan))
    if CONF_CMDSCAN_STOP_BTN in config:
        btn = await cg.get_variable(config[CONF_CMDSCAN_STOP_BTN])
        cg.add(btn.add_on_press(var.stop_command_scan))

    # Link numbers to setters
    if CONF_BRIGHTNESS_NUMBER in config:
        num = await cg.get_variable(config[CONF_BRIGHTNESS_NUMBER])
        cg.add(num.add_on_state(var.set_brightness))
    if CONF_CMDSCAN_START in config:
        num = await cg.get_variable(config[CONF_CMDSCAN_START])
        cg.add(var.set_cmdscan_start_number(num))
    if CONF_CMDSCAN_END in config:
        num = await cg.get_variable(config[CONF_CMDSCAN_END])
        cg.add(var.set_cmdscan_end_number(num))
    if CONF_CMDSCAN_PAUSE in config:
        num = await cg.get_variable(config[CONF_CMDSCAN_PAUSE])
        cg.add(var.set_cmdscan_pause_number(num))

    # Link sensor for last scanned command
    if CONF_LAST_SCANNED_SENSOR in config:
        s = await cg.get_variable(config[CONF_LAST_SCANNED_SENSOR])
        cg.add(var.set_last_scanned_sensor(s))
