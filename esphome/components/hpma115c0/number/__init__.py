import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_CONFIG,
    ICON_GAUGE,
    UNIT_EMPTY,
)

from .. import CONF_HPMA115C0_ID, Hpma115C0PollingComponent, hpma115c0_ns

Hpma115C0AdjustmentNumber = hpma115c0_ns.class_(
    "Hpma115C0AdjustmentNumber", number.Number
)

CONF_ADJUSTMENT_COEFFICIENT = "adjustment_coefficient"
CALIBRATION_GROUP = "calibration_group"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HPMA115C0_ID): cv.use_id(Hpma115C0PollingComponent),
        cv.Optional(CONF_ADJUSTMENT_COEFFICIENT): number.number_schema(
            Hpma115C0AdjustmentNumber,
            device_class=DEVICE_CLASS_EMPTY,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_GAUGE,
            unit_of_measurement=UNIT_EMPTY,
        ),
    }
)


async def to_code(config):
    Hpma115C0_polling_component = await cg.get_variable(config[CONF_HPMA115C0_ID])
    if adjustment_coefficient_config := config.get(CONF_ADJUSTMENT_COEFFICIENT):
        # Limit range of number between min and max for adjustment coefficient
        adjustment_min_value = (
            Hpma115C0_polling_component.get_adjustment_coefficient_min_value()
        )
        adjustment_max_value = (
            Hpma115C0_polling_component.get_adjustment_coefficient_max_value()
        )
        n = await number.new_number(
            adjustment_coefficient_config,
            min_value=adjustment_min_value,
            max_value=adjustment_max_value,
            step=1,
        )
        await cg.register_parented(n, config[CONF_HPMA115C0_ID])
        cg.add(Hpma115C0_polling_component.set_adjustment_coefficient(n))
