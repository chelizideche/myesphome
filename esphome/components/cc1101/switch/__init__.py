import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG

from .. import (
    CONF_AGC,
    CONF_CC1101_ID,
    CONF_LNA_PRIORITY,
    CC1101Component,
    for_each_conf,
    ns,
)

AgcLnaPrioritySwitch = ns.class_("AgcLnaPrioritySwitch", switch.Switch)

AGC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LNA_PRIORITY): switch.switch_schema(
            AgcLnaPrioritySwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
        cv.Optional(CONF_AGC): AGC_SCHEMA,
    }
)

VARIABLES = {
    None: [],
    CONF_AGC: [
        [CONF_LNA_PRIORITY],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    async def new_switch(c, args, setter):
        s = await switch.new_switch(c)
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_switch")(s))

    await for_each_conf(config, VARIABLES, new_switch)
