import esphome.codegen as cg
from esphome.components.canbus import CanbusComponent

esp32_can_ns = cg.esphome_ns.namespace("esp32_can")
esp32_can = esp32_can_ns.class_("ESP32Can", CanbusComponent)
TXMode = esp32_can_ns.enum("TXMode", is_class=True)
