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


# --- AFTER_DECLARE pattern for linking template entities to component actions ---
def after_declare():
    # Buttons
    cg.add_global(cg.Statement('esphome::App.register_post_setup([] {'))
    cg.add_global(cg.Statement('  auto *comp = id(hamulight_component);'))
    cg.add_global(cg.Statement('  if (App.get_button_by_id("hamulight_toggle_btn")) App.get_button_by_id("hamulight_toggle_btn")->set_on_press([comp] { comp->toggle(); });'))
    cg.add_global(cg.Statement('  if (App.get_button_by_id("hamulight_pair_btn")) App.get_button_by_id("hamulight_pair_btn")->set_on_press([comp] { comp->pair_with_driver(); });'))
    cg.add_global(cg.Statement('  if (App.get_button_by_id("hamulight_cmdscan_btn")) App.get_button_by_id("hamulight_cmdscan_btn")->set_on_press([comp] { comp->start_command_scan(); });'))
    cg.add_global(cg.Statement('  if (App.get_button_by_id("hamulight_cmdscan_stop_btn")) App.get_button_by_id("hamulight_cmdscan_stop_btn")->set_on_press([comp] { comp->stop_command_scan(); });'))
    # Numbers
    cg.add_global(cg.Statement('  if (App.get_number_by_id("hamulight_brightness")) {'))
    cg.add_global(cg.Statement('    auto *n = App.get_number_by_id("hamulight_brightness");'))
    cg.add_global(cg.Statement('    n->set_min_value(0); n->set_max_value(100); n->set_step(1);'))
    cg.add_global(cg.Statement('    n->set_set_action([comp](float x) { comp->set_brightness(x); });'))
    cg.add_global(cg.Statement('  }'))
    cg.add_global(cg.Statement('  if (App.get_number_by_id("hamulight_cmdscan_start")) comp->set_cmdscan_start_number(App.get_number_by_id("hamulight_cmdscan_start"));'))
    cg.add_global(cg.Statement('  if (App.get_number_by_id("hamulight_cmdscan_end")) comp->set_cmdscan_end_number(App.get_number_by_id("hamulight_cmdscan_end"));'))
    cg.add_global(cg.Statement('  if (App.get_number_by_id("hamulight_cmdscan_pause")) comp->set_cmdscan_pause_number(App.get_number_by_id("hamulight_cmdscan_pause"));'))
    cg.add_global(cg.Statement('  if (App.get_sensor_by_id("hamulight_last_scanned_command")) comp->set_last_scanned_sensor(App.get_sensor_by_id("hamulight_last_scanned_command"));'))
    cg.add_global(cg.Statement('});')
    )

after_declare()
