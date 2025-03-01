from collections.abc import MutableMapping

import esphome.codegen as cg
from esphome.components import time as time_
from esphome.components.zigbee_ctx import (
    KEY_EP_NUMBER,
    consume_ep_slots,
    zigbee_register_ep,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority

from .. import (
    ZigbeeBaseSchema,
    zigbee_new_attr_list,
    zigbee_new_cluster_list,
    zigbee_new_variable,
)
from ..const import (
    CONF_BASIC_ATTRIB_LIST_EXT,
    CONF_EP,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_SCENES_ATTRIB_LIST,
    CONF_SERVER,
    CONF_TIME_ATTRIB_LIST,
    CONF_TIME_ATTRS,
    CONF_TIME_CLUSTER_LIST,
    CONF_ZIGBEE_ID,
    esphome_zb_ha_declare_time_ep,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeTime = zigbee_ns.class_("ZigbeeTime", time_.RealTimeClock)

zb_zcl_time_attrs_t = cg.global_ns.struct("zb_zcl_time_attrs_t")


def _consume_ep_slots(config: MutableMapping) -> MutableMapping:
    if config[CONF_SERVER]:
        consume_ep_slots(config)
    return config


CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeTime),
            cv.GenerateID(CONF_TIME_ATTRS): cv.declare_id(zb_zcl_time_attrs_t),
            cv.GenerateID(CONF_TIME_ATTRIB_LIST): cv.declare_id(
                cg.global_ns.namespace("ZB_ZCL_DECLARE_TIME_ATTR_LIST")
            ),
            cv.GenerateID(CONF_TIME_CLUSTER_LIST): cv.declare_id(
                cg.global_ns.namespace("ESPHOME_ZB_HA_DECLARE_TIME_CLUSTER_LIST")
            ),
            cv.GenerateID(CONF_EP): cv.declare_id(esphome_zb_ha_declare_time_ep),
            cv.Optional(CONF_SERVER, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ZigbeeBaseSchema)
    .extend(cv.polling_component_schema("1s")),
    _consume_ep_slots,
)


@coroutine_with_priority(50.0)
async def to_code(config):
    if config[CONF_SERVER]:
        cg.add_define("USE_ZIGBEE_TIME_SERVER")
        time_attrs = zigbee_new_variable(config[CONF_TIME_ATTRS])
        attr_list = zigbee_new_attr_list(
            config[CONF_TIME_ATTRIB_LIST],
            time_attrs,
        )

        cluster = zigbee_new_cluster_list(
            config[CONF_TIME_CLUSTER_LIST],
            attr_list,
            config[CONF_BASIC_ATTRIB_LIST_EXT],
            config[CONF_IDENTIFY_ATTRIB_LIST],
            config[CONF_GROUPS_ATTRIB_LIST],
            config[CONF_SCENES_ATTRIB_LIST],
        )

        ep = config[KEY_EP_NUMBER]
        zigbee_register_ep(config[CONF_EP], cluster, ep)
    else:
        ep = 1

    var = cg.new_Pvariable(config[CONF_ID])
    await time_.register_time(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_ep(ep))
    if config[CONF_SERVER]:
        cg.add(var.set_cluster_attributes(time_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))
