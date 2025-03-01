from collections.abc import MutableMapping
from typing import Any

import esphome.codegen as cg
from esphome.components.nrf52 import add_pm_static
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
from esphome.core import CORE, ID, coroutine_with_priority

KEY_ZIGBEE = "zigbee"
KEY_EP_NUMBER = "ep_number"
CONF_EP = "ep"


def zigbee_set_core_data(config):
    if CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


def consume_ep_slots(config: MutableMapping) -> MutableMapping:
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    ep = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    config[KEY_EP_NUMBER] = ep
    return config


def zigbee_register_ep(id_: ID, cluster, ep: int):
    assert isinstance(id_, ID)
    CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER][ep - 1] = str(id_)
    obj = cg.RawExpression(f"{id_.type}({id_}, {ep}, {cluster})")
    CORE.add_global(obj)


@coroutine_with_priority(10.0)
async def to_code(config):
    if len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER]) > 0:
        cg.add_global(
            cg.RawExpression(
                f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])})"
            )
        )
        cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))
