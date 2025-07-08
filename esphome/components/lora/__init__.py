"""ESPHome LoRa component."""

import logging

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_ON_MESSAGE, CONF_PLATFORM

CODEOWNERS = ["@dala318"]

lora_ns = cg.esphome_ns.namespace("lora")
LoRa = lora_ns.class_("LoRa", cg.Component)
LoRaListener = lora_ns.class_("LoRaListener")

# RunImageCalAction = lora_ns.class_(
#     "RunImageCalAction", automation.Action, cg.Parented.template(LoRa)
# )
SendPacketAction = lora_ns.class_(
    "SendPacketAction", automation.Action, cg.Parented.template(LoRa)
)
SetModeTxAction = lora_ns.class_(
    "SetModeTxAction", automation.Action, cg.Parented.template(LoRa)
)
SetModeRxAction = lora_ns.class_(
    "SetModeRxAction", automation.Action, cg.Parented.template(LoRa)
)
SetModeSleepAction = lora_ns.class_(
    "SetModeSleepAction", automation.Action, cg.Parented.template(LoRa)
)
SetModeStandbyAction = lora_ns.class_(
    "SetModeStandbyAction", automation.Action, cg.Parented.template(LoRa)
)

IS_PLATFORM_COMPONENT = True

DOMAIN = "lora"

CONF_LORA_ID = "lora_id"


_LOGGER = logging.getLogger(__name__)


LORA_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LORA_ID): cv.use_id(LoRa),
    }
)


LORA_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def register_lora_client(var, config):
    lora_var = await cg.get_variable(config[CONF_LORA_ID])
    cg.add(var.set_parent(lora_var))
    return lora_var


def lora_schema(cls):
    return LORA_COMPONENT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(cls)})


async def new_lora(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_platform_name(config[CONF_PLATFORM]))
    await cg.register_component(var, config)
    if CONF_ON_MESSAGE in config:
        await automation.build_automation(
            var.get_packet_trigger(),
            [
                (cg.std_vector.template(cg.uint8), "x"),
                (cg.float_, "rssi"),
                (cg.float_, "snr"),
            ],
            config[CONF_ON_MESSAGE],
        )
    return var


NO_ARGS_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(LoRa),
    }
)


# @automation.register_action(
#     "lora.run_image_cal", RunImageCalAction, NO_ARGS_ACTION_SCHEMA
# )
@automation.register_action("lora.set_mode_tx", SetModeTxAction, NO_ARGS_ACTION_SCHEMA)
@automation.register_action("lora.set_mode_rx", SetModeRxAction, NO_ARGS_ACTION_SCHEMA)
@automation.register_action(
    "lora.set_mode_sleep", SetModeSleepAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "lora.set_mode_standby", SetModeStandbyAction, NO_ARGS_ACTION_SCHEMA
)
async def no_args_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


SEND_PACKET_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(LoRa),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    },
    key=CONF_DATA,
)


@automation.register_action(
    "lora.send_packet", SendPacketAction, SEND_PACKET_ACTION_SCHEMA
)
async def send_packet_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)
    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var
