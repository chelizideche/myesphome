import esphome.codegen as cg
from esphome.components.nrf52 import add_pm_static
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
from esphome.core import CORE, coroutine_with_priority

KEY_ZIGBEE = "zigbee"
KEY_EP = "ep"


def zigbee_set_core_data(config):
    print(CORE.data)
    CORE.data[KEY_ZIGBEE] = {}
    CORE.data[KEY_ZIGBEE][KEY_EP] = []

    if CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


@coroutine_with_priority(10.0)
async def to_code(config):
    if len(CORE.data[KEY_ZIGBEE][KEY_EP]) > 0:
        cg.add_global(
            cg.RawExpression(
                f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP])})"
            )
        )
        cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))
