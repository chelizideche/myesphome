# import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv

# from esphome.const import (
#     CONF_ENTITY_CATEGORY,
#     CONF_ICON,
#     CONF_ID,
#     CONF_MODE,
#     CONF_SOURCE_ID,
#     ENTITY_CATEGORY_CONFIG,
#     ICON_MOTION_SENSOR,
#     ICON_PULSE,
#     ICON_TIMELAPSE,
# )
# from esphome.core.entity_helpers import inherit_property_from
from .. import ld2410s_ns

CODEOWNERS = ["@NovakIrs"]

LD2410SThresholdTriggerText = ld2410s_ns.class_(
    "LD2410SThresholdTriggerText", text.Text
)

CONF_THRESHOLDS_TRIGGER_TEXT = "thresholds_trigger"
CONF_THRESHOLDS_HOLD_TEXT = "thrEsholds_hold"
CONF_THRESHOLDS_SNR_TEXT = "thrEsholds_snr"


CONFIG_SCHEMA = text.TEXT_SCHEMA.extend(
    {
        cv.Required(CONF_THRESHOLDS_TRIGGER_TEXT): cv.declare_id(
            LD2410SThresholdTriggerText
        ),
    }
).extend(cv.COMPONENT_SCHEMA)
