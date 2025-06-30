import esphome.codegen as cg
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["md5"]

ota_base_ns = cg.esphome_ns.namespace("ota_base")


@coroutine_with_priority(52.0)
async def to_code(config):
    # Note: USE_OTA_STATE_CALLBACK is not defined here
    # Components that need OTA callbacks (like esp32_ble_tracker, speaker, etc.)
    # define USE_OTA_STATE_CALLBACK themselves in their own __init__.py files
    # This ensures the callback functionality is only compiled when actually needed

    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("Update", None)

    if CORE.is_rp2040 and CORE.using_arduino:
        cg.add_library("Updater", None)
