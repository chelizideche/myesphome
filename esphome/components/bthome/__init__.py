import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_MAC_ADDRESS, CONF_ID


CODEOWNERS = ["@badrpc"]
DEPENDENCIES = ["esp32_ble_tracker"]
MULTI_CONF = True

CONF_ENCRYPTION_KEY = "encryption_key"

bthome_ns = cg.esphome_ns.namespace("bthome")
BTHome = bthome_ns.class_("BTHome", esp32_ble_tracker.ESPBTDeviceListener, cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BTHome),
            cv.Optional(CONF_ENCRYPTION_KEY): cv.bind_key,
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    if CONF_ENCRYPTION_KEY in config:
         cg.add(var.set_encryption_key(config[CONF_ENCRYPTION_KEY]))
