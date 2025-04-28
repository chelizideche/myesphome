import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ICON_RESTART

from .. import BL0940, CONF_BL0940_ID, bl0940_ns

CalibrationResetButton = bl0940_ns.class_(
    "CalibrationResetButton", button.Button, cg.Component
)

CONFIG_SCHEMA = cv.All(
    button.button_schema(
        CalibrationResetButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART,
    )
    .extend({cv.Required(CONF_BL0940_ID): cv.use_id(BL0940)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    bl0940 = await cg.get_variable(config[CONF_BL0940_ID])

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)

    cg.add(bl0940.set_reset_calibration_button(var))
