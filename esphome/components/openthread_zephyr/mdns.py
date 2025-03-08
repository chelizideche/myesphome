import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mdns
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority

from .const import openthread_zephyr_ns

DEPENDENCIES = ["openthread_zephyr"]
AUTO_LOAD = ["mdns"]

# This component acts as a bridge between ESPHome's mDNS component and OpenThread's SRP client
OpenThreadMDNSBridge = openthread_zephyr_ns.class_("OpenThreadMDNSBridge", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(OpenThreadMDNSBridge),
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(800)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Ensure mDNS component is loaded and get a reference to it
    if not CORE.has_id(mdns.CONFIG_SCHEMA["id"]):
        conf = mdns.CONFIG_SCHEMA.copy()
        conf[CONF_ID] = mdns.CONFIG_SCHEMA["id"]
        await mdns.to_code(conf)
    
    # Get reference to the mDNS component
    mdns_component = await cg.get_variable(mdns.CONFIG_SCHEMA["id"])
    
    # Link the mDNS component to our bridge
    cg.add(var.set_mdns(mdns_component)) 