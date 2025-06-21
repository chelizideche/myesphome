import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_ID, CONF_NAME, CONF_LIGHT_ID

# Import the HamulightComponent from __init__.py
from . import HAMULIGHT_NAMESPACE, HamulightComponent

# Define a constant for the ID linking to the HamulightComponent instance
CONF_HAMULIGHT_ID = "hamulight_id"

# Configuration schema for the light platform
# Defines the options available in YAML under 'light: - platform: hamulight'
CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_LIGHT_ID): cv.declare_id(light.LightOutput), # Generates an ID for the HamulightComponent C++ instance
    cv.Required(CONF_HAMULIGHT_ID): cv.use_id(HamulightComponent),  # Links this light to an existing HamulightComponent instance
}).extend(cv.COMPONENT_SCHEMA) # Extends with standard ESPHome component options

# Code generation function for the light platform
async def to_code(config):
    # Gets the referenced HamulightComponent instance from __init__.py (via hamulight_id)
    paren = await cg.get_variable(config[CONF_HAMULIGHT_ID])

    # Registers the HamulightComponent instance as LightOutput (no new variable needed)
    await light.register_light(paren, config)
