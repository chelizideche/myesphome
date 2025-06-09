import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_FACTORY_RESET, ENTITY_CATEGORY_CONFIG, ICON_DATABASE

from .. import CONF_HPMA115C0_ID, Hpma115C0PollingComponent, hpma115c0_ns

Hpma115C0FactoryDefaultButton = hpma115c0_ns.class_(
    "Hpma115C0FactoryDefaultButton", button.Button
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_HPMA115C0_ID): cv.use_id(Hpma115C0PollingComponent),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        Hpma115C0FactoryDefaultButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_DATABASE,
    ),
}


async def to_code(config):
    Hpma115C0_polling_component = await cg.get_variable(config[CONF_HPMA115C0_ID])
    if factory_reset := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset)
        await cg.register_parented(b, config[CONF_HPMA115C0_ID])
        cg.add(Hpma115C0_polling_component.set_factory_default_button(b))
