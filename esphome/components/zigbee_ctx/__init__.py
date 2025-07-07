from collections.abc import MutableMapping
from typing import Any

import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_pm_static
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
from esphome.core import CORE, coroutine_with_priority

KEY_ZIGBEE = "zigbee"
KEY_EP_NUMBER = "ep_number"
CONF_EP = "ep"


def zigbee_set_core_data(config):
    if CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


def consume_ep_slots(config: MutableMapping) -> MutableMapping:
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    config[KEY_EP_NUMBER] = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    return config


def zigbee_register_ep(
    config,
    cluster_id,
    report_attr_count: int,
    clusters,
):
    id_ = config[CONF_EP]
    in_cluster_num = 0
    out_cluster_num = 0
    attrs = []
    for c in clusters:
        if c.attr:
            in_cluster_num += 1
        else:
            out_cluster_num += 1
        attrs.append(c.name)
    CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER][config[KEY_EP_NUMBER] - 1] = str(id_)
    obj = cg.RawExpression(
        f"{id_.type}({id_}, {config[KEY_EP_NUMBER]}, {cluster_id}, {in_cluster_num}, {out_cluster_num}, {report_attr_count}, {', '.join(attrs)})"
    )
    CORE.add_global(obj)


@coroutine_with_priority(10.0)
async def to_code(config):
    cg.add_global(
        cg.RawExpression(
            f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])})"
        )
    )
    cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))
