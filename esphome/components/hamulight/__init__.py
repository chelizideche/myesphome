import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins

from esphome.components.button import register_template_button
from esphome.components.number import register_template_number
from esphome.components.sensor import register_template_sensor

HAMULIGHT_NAMESPACE = cg.esphome_ns.namespace('hamulight')
HamulightComponent = HAMULIGHT_NAMESPACE.class_('HamulightComponent', cg.Component)

CONF_RF_TRANSMIT_PIN = "rf_transmit_pin"
CONF_RF_ADDRESS = "rf_address"
CONF_LED_PIN = "led_pin"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HamulightComponent),
    cv.Required(CONF_RF_TRANSMIT_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_LED_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_RF_ADDRESS): cv.hex_uint16_t,
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
    cg.add(var.set_rf_address(config[CONF_RF_ADDRESS]))

    # Register template buttons and their actions
    button_actions = {
        "hamulight_toggle_btn": var.toggle,
        "hamulight_pair_btn": var.pair_with_driver,
        "hamulight_cmdscan_btn": var.start_command_scan,
        "hamulight_cmdscan_stop_btn": var.stop_command_scan,
    }
    for btn_id, method in button_actions.items():
        await register_template_button(
            var, btn_id, on_press=method
        )

    # Register template numbers and their actions
    number_actions = {
        "hamulight_brightness": var.set_brightness,
        "hamulight_cmdscan_start": var.set_cmdscan_start_number,
        "hamulight_cmdscan_end": var.set_cmdscan_end_number,
        "hamulight_cmdscan_pause": var.set_cmdscan_pause_number,
    }
    for num_id, method in number_actions.items():
        await register_template_number(
            var, num_id, set_action=method
        )

    # Register template sensor for last scanned command
    await register_template_sensor(
        var, "hamulight_last_scanned_command", method=var.set_last_scanned_sensor
    )
