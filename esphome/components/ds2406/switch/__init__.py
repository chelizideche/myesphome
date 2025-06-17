import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL

from .. import CONF_DS2406_ID, Ds2406, ds2406_ns

DEPENDENCIES = ["ds2406"]

Ds2406Switch = ds2406_ns.class_("Ds2406Switch", switch.Switch)

CHANNELS = [1, 2]

CONFIG_SCHEMA = switch.switch_schema(Ds2406Switch).extend(
    {
        cv.GenerateID(CONF_DS2406_ID): cv.use_id(Ds2406),
        cv.Required(CONF_CHANNEL): cv.one_of(*CHANNELS, int=True),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_parented(var, config[CONF_DS2406_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
