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
    ICON_RESTART_ALERT,
)

from .. import CONF_LD2410S_ID, LD2410S, ld2410s_ns

CODEOWNERS = ["@NovakIrs"]

LD2410SReadAll = ld2410s_ns.class_("LD2410SReadAll", button.Button)
LD2410SApplyConfigButton = ld2410s_ns.class_("LD2410SApplyConfigButton", button.Button)
LD2410SCalibration = ld2410s_ns.class_("LD2410SCalibration", button.Button)
LD2410SFactoryReset = ld2410s_ns.class_("LD2410SFactoryReset", button.Button)
LD2410SMinimalOutput = ld2410s_ns.class_("LD2410SMinimalOutput", button.Button)
# LD2410SEnableConfigButton = ld2410s_ns.class_("LD2410SEnableConfigButton", button.Button)
# LD2410SDisableConfigButton = ld2410s_ns.class_("LD2410SDisableConfigButton", button.Button)

CONF_READ_ALL = "read_all"
CONF_APPLY_CONFIG = "apply_config"
# CONF_CALIBRATION = "calibration"
# CONF_FACTORY_RESET = "factory_reset"
CONF_MINIMAL_OUTPUT = "minimal_output"
# CONF_ENABLE_CONFIG = "enable_config"
# CONF_DISABLE_CONFIG = "disable_config"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Required(CONF_APPLY_CONFIG): button.button_schema(
        LD2410SApplyConfigButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Required(CONF_CALIBRATION): button.button_schema(
        LD2410SCalibration,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:icon-refresh",
    ),
    cv.Required(CONF_READ_ALL): button.button_schema(
        LD2410SReadAll,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:icon-refresh",
    ),
    cv.Required(CONF_FACTORY_RESET): button.button_schema(
        LD2410SFactoryReset,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:icon-refresh",
    ),
    cv.Required(CONF_MINIMAL_OUTPUT): button.button_schema(
        LD2410SMinimalOutput,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:icon-refresh",
    ),
    # ,
    # cv.Optional(CONF_ENABLE_CONFIG): button.button_schema(
    #     LD2410SEnableConfigButton,
    #     device_class=DEVICE_CLASS_IDENTIFY,
    #     entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    #     icon=ICON_BUG
    # ),
    # cv.Optional(CONF_DISABLE_CONFIG): button.button_schema(
    #     LD2410SDisableConfigButton,
    #     device_class=DEVICE_CLASS_IDENTIFY,
    #     entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    #     icon=ICON_BUG
    # )
}


async def to_code(config):
    ld2410s_component = await cg.get_variable(config[CONF_LD2410S_ID])

    if read_all := config.get(CONF_READ_ALL):
        b = await button.new_button(read_all)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_read_all_button(b))

    if apply_config := config.get(CONF_APPLY_CONFIG):
        b = await button.new_button(apply_config)
        await cg.register_parented(b, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_apply_config_button(b))

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


# toggle_minimal_output
# set_toggle_minimal_output_button

# if enable_confing := config.get(CONF_ENABLE_CONFIG):
#     b = await button.new_button(enable_confing)
#     await cg.register_parented(b, config[CONF_LD2410S_ID])
#     cg.add(ld2410s_component.set_enable_config_button(b))
# if disable_confing := config.get(CONF_DISABLE_CONFIG):
#     b = await button.new_button(disable_confing)
#     await cg.register_parented(b, config[CONF_LD2410S_ID])
#     cg.add(ld2410s_component.set_disable_config_button(b))
