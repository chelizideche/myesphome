import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_WAIT_TIME, ENTITY_CATEGORY_CONFIG

from .. import (
    CARRIER_SENSE_REL_THR,
    CONF_AGC,
    CONF_CARRIER_SENSE_REL_THR,
    CONF_CC1101_ID,
    CONF_FILTER_LENGTH_ASK_OOK,
    CONF_FILTER_LENGTH_FSK_MSK,
    CONF_FREEZE,
    CONF_HYST_LEVEL,
    CONF_MAGN_TARGET,
    CONF_MAX_DVGA_GAIN,
    CONF_MAX_LNA_GAIN,
    CONF_MODULATION,
    CONF_RX_ATTENUATION,
    CONF_TUNER,
    FILTER_LENGTH_ASK_OOK,
    FILTER_LENGTH_FSK_MSK,
    FREEZE,
    HYST_LEVEL,
    MAGN_TARGET,
    MAX_DVGA_GAIN,
    MAX_LNA_GAIN,
    MODULATION,
    RX_ATTENUATION,
    WAIT_TIME,
    CC1101Component,
    for_each_conf,
    ns,
)

RxAttenuationSelect = ns.class_("RxAttenuationSelect", select.Select)
TunerModulationSelect = ns.class_("TunerModulationSelect", select.Select)
AgcMagnTargetSelect = ns.class_("AgcMagnTargetSelect", select.Select)
AgcMaxLnaGainSelect = ns.class_("AgcMaxLnaGainSelect", select.Select)
AgcMaxDvgaGainSelect = ns.class_("AgcMaxDvgaGainSelect", select.Select)
AgcCarrierSenseRelThrSelect = ns.class_("AgcCarrierSenseRelThrSelect", select.Select)
AgcFilterLengthFskMskSelect = ns.class_("AgcFilterLengthFskMskSelect", select.Select)
AgcFilterLengthAskOokSelect = ns.class_("AgcFilterLengthAskOokSelect", select.Select)
AgcFreezeSelect = ns.class_("AgcFreezeSelect", select.Select)
AgcWaitTimeSelect = ns.class_("AgcWaitTimeSelect", select.Select)
AgcHystLevelSelect = ns.class_("AgcHystLevelSelect", select.Select)

TUNER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MODULATION): select.select_schema(
            TunerModulationSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
    }
)

AGC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MAGN_TARGET): select.select_schema(
            AgcMagnTargetSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_MAX_LNA_GAIN): select.select_schema(
            AgcMaxLnaGainSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_MAX_DVGA_GAIN): select.select_schema(
            AgcMaxDvgaGainSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_CARRIER_SENSE_REL_THR): select.select_schema(
            AgcCarrierSenseRelThrSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_FILTER_LENGTH_FSK_MSK): select.select_schema(
            AgcFilterLengthFskMskSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_FILTER_LENGTH_ASK_OOK): select.select_schema(
            AgcFilterLengthAskOokSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_FREEZE): select.select_schema(
            AgcFreezeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_WAIT_TIME): select.select_schema(
            AgcWaitTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_HYST_LEVEL): select.select_schema(
            AgcHystLevelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
        cv.Optional(CONF_RX_ATTENUATION): select.select_schema(
            RxAttenuationSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_TUNER): TUNER_SCHEMA,
        cv.Optional(CONF_AGC): AGC_SCHEMA,
    }
)

VARIABLES = {
    None: [[CONF_RX_ATTENUATION, RX_ATTENUATION]],
    CONF_TUNER: [
        [CONF_MODULATION, MODULATION],
    ],
    CONF_AGC: [
        [CONF_MAGN_TARGET, MAGN_TARGET],
        [CONF_MAX_LNA_GAIN, MAX_LNA_GAIN],
        [CONF_MAX_DVGA_GAIN, MAX_DVGA_GAIN],
        [CONF_CARRIER_SENSE_REL_THR, CARRIER_SENSE_REL_THR],
        [CONF_FILTER_LENGTH_FSK_MSK, FILTER_LENGTH_FSK_MSK],
        [CONF_FILTER_LENGTH_ASK_OOK, FILTER_LENGTH_ASK_OOK],
        [CONF_FREEZE, FREEZE],
        [CONF_WAIT_TIME, WAIT_TIME],
        [CONF_HYST_LEVEL, HYST_LEVEL],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    async def new_select(c, args, setter):
        s = await select.new_select(c, options=list(args[1].keys()))
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_select")(s))

    await for_each_conf(config, VARIABLES, new_select)
