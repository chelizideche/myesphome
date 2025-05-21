import esphome.codegen as cg
from esphome.components.canbus import CanbusComponent

esp32_can_ns = cg.esphome_ns.namespace("esp32_can")
ESP32Can = esp32_can_ns.class_("ESP32Can", CanbusComponent)
