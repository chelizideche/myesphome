import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_FACTORY_RESET,
    # DEVICE_CLASS_IDENTIFY,
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_UPDATE,
    ENTITY_CATEGORY_CONFIG,
    # ENTITY_CATEGORY_DIAGNOSTIC,
    # ICON_BUG,
    # ICON_MEMORY,
)

from .. import CONF_LD2410S_ID, LD2410S, ld2410s_ns

CODEOWNERS = ["@NovakIrs"]

LD2410SReadAll = ld2410s_ns.class_("LD2410SReadAll", button.Button)
LD2410SApplyConfigButton = ld2410s_ns.class_("LD2410SApplyConfigButton", button.Button)
LD2410SCalibration = ld2410s_ns.class_("LD2410SCalibration", button.Button)
LD2410SFactoryReset = ld2410s_ns.class_("LD2410SFactoryReset", button.Button)
LD2410SMinimalOutput = ld2410s_ns.class_("LD2410SMinimalOutput", button.Button)

CONF_READ_ALL = "read_all"
CONF_WRITE_ALL = "write_all"
# CONF_CALIBRATION = "calibration"
# CONF_FACTORY_RESET = "factory_reset"
CONF_MINIMAL_OUTPUT = "minimal_output"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Required(CONF_READ_ALL): button.button_schema(
        LD2410SReadAll,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:reload",
    ),
    cv.Required(CONF_WRITE_ALL): button.button_schema(
        LD2410SApplyConfigButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:content-save",
    ),
    cv.Required(CONF_CALIBRATION): button.button_schema(
        LD2410SCalibration,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:refresh-auto",
    ),
    cv.Required(CONF_FACTORY_RESET): button.button_schema(
        LD2410SFactoryReset,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:factory",
    ),
    cv.Required(CONF_MINIMAL_OUTPUT): button.button_schema(
        LD2410SMinimalOutput,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:arrow-collapse-horizontal",
    ),
}


async def to_code(config):
    ld2410s_component = await cg.get_variable(config[CONF_LD2410S_ID])

    if read_all := config.get(CONF_READ_ALL):
        b = await button.new_button(read_all)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_read_all_button(b))

    if write_all := config.get(CONF_WRITE_ALL):
        b = await button.new_button(write_all)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_write_all_button(b))

    if calibration := config.get(CONF_CALIBRATION):
        b = await button.new_button(calibration)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_calibration_button(b))

    if factory_reset := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_factory_reset_button(b))

    if toggle_minimal_output := config.get(CONF_MINIMAL_OUTPUT):
        b = await button.new_button(toggle_minimal_output)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_toggle_minimal_output_button(b))
