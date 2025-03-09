import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_CHIP  # noqa: F401

from .. import CONF_ES8388_ID, ES8388_BASE_SCHEMA, ES8388Hub, es8388_ns

CONF_DAC_OUTPUT = "dac_output"

DacOutputSelect = es8388_ns.class_("DacOutputSelect", select.Select)

CONFIG_SCHEMA = cv.All(
    {
        cv.GenerateID(CONF_ES8388_ID): cv.use_id(ES8388Hub),
        cv.Optional(CONF_DAC_OUTPUT): select.select_schema(
            DacOutputSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_CHIP,
        ).extend(ES8388_BASE_SCHEMA),
    }
)


async def to_code(config):
    es8388_hub = await cg.get_variable(config[CONF_ES8388_ID])
    if dac_output_config := config.get(CONF_DAC_OUTPUT):
        s = await select.new_select(
            dac_output_config,
            options=["LINE1", "LINE2", "BOTH"],
        )
        await cg.register_parented(s, config[CONF_ES8388_ID])
        cg.add(es8388_hub.set_dac_output_select(s))
