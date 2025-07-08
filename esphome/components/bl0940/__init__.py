import esphome.codegen as cg
from esphome.components import uart

CODEOWNERS = ["@tobias-", "@dan-s-github"]

DEPENDENCIES = ["uart"]

CONF_BL0940_ID = "bl0940_id"
bl0940_ns = cg.esphome_ns.namespace("bl0940")
BL0940 = bl0940_ns.class_("BL0940", cg.PollingComponent, uart.UARTDevice)
