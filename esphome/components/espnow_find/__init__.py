from esphome import automation
import esphome.codegen as cg
from esphome.components.espnow import (
    CONF_DEFAULT,
    CONF_ON_FAILED,
    CONF_ON_SUCCEED,
    ESPNOW_APP_SCHEMA,
    ESPNowInterface,
    espnow_ns,
    register_app,
    register_peer,
    validate_peer,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAC_ADDRESS

# import esphome.final_validate as fv

CODEOWNERS = ["@nielsnl68"]

DEPENDENCIES = ["espnow"]


ESPNowFindPeer = espnow_ns.class_("ESPNowFindPeer", ESPNowInterface)
FindPeerAction = espnow_ns.class_("FindPeerAction", automation.Action, cg.Component)

CONFIG_SCHEMA = ESPNOW_APP_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ESPNowFindPeer),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_app(var, config)


@automation.register_action(
    "espnow.peer.find",
    FindPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowFindPeer),
            cv.Optional(CONF_MAC_ADDRESS, default=CONF_DEFAULT): validate_peer,
            cv.Optional(CONF_ON_SUCCEED): automation.validate_action_list,
            cv.Optional(CONF_ON_FAILED): automation.validate_action_list,
        },
        key=CONF_MAC_ADDRESS,
    ),
)
async def channel_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    await register_peer(var, config, args)

    if CONF_ON_SUCCEED in config:
        actions = await automation.build_action_list(
            config[CONF_ON_SUCCEED], template_arg, args
        )
        cg.add(var.add_on_succeed(actions))
    if CONF_ON_FAILED in config:
        actions = await automation.build_action_list(
            config[CONF_ON_FAILED], template_arg, args
        )
        cg.add(var.add_on_failed(actions))
    return var
