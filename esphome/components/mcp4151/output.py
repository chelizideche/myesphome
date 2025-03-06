import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, output
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]

mcp4151_ns = cg.esphome_ns.namespace("mcp4151")
MCP4151 = mcp4151_ns.class_("MCP4151", cg.Component, output.FloatOutput, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MCP4151),
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema())

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
    await output.register_output(var, config)
