from esphome import automation, core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_PAYLOAD,
    CONF_TRIGGER_ID,
    CONF_WIFI,
)
from esphome.core import CORE
import esphome.final_validate as fv

CODEOWNERS = ["@nielsnl68", "@jesserockz"]


espnow_ns = cg.esphome_ns.namespace("espnow")
ESPNowComponent = espnow_ns.class_("ESPNowComponent", cg.Component)
ESPNowApp = espnow_ns.class_("ESPNowApp")


ESPNowListener = espnow_ns.class_("ESPNowListener")

ESPNowPacket = espnow_ns.class_("ESPNowPacket")

ESPNowInterface = espnow_ns.class_(
    "ESPNowInterface", cg.Component, cg.Parented.template(ESPNowComponent)
)

std_weak_ptr = cg.std_ns.class_("weak_ptr")

ESPNowDefaultTrigger = espnow_ns.class_(
    "ESPNowDefaultTrigger",
    automation.Trigger.template(std_weak_ptr.template(ESPNowPacket)),
)

SendAction = espnow_ns.class_("SendAction", automation.Action)

NewPeerAction = espnow_ns.class_("NewPeerAction", automation.Action)
DelPeerAction = espnow_ns.class_("DelPeerAction", automation.Action)

SetChannelAction = espnow_ns.class_("SetChannelAction", automation.Action)


CONF_AUTO_ADD_PEER = "auto_add_peer"
CONF_CONFORMATION_TIMEOUT = "conformation_timeout"
CONF_ESPNOW = "espnow"
CONF_APP_ID = "app_id"
CONF_ATTEMPTS = "attempts"
CONF_DEFAULT = "default"

CONF_PEER = "peer"
CONF_PEER_ID = "peer_id"
CONF_PREDEFINED_PEERS = "predefined_peers"
CONF_WAIT_FOR_ACK = "wait_for_ack"
CONF_WIFI_CHANNEL = "wifi_channel"
CONF_CUSTOM_APPS = "custom_apps"
CONF_DEFAULT_APP_ID = "default_app_id"
CONF_DONT_WAIT = "dont_wait"

CONF_ON_NEW_PEER = "on_new_peer"
CONF_ON_RECEIVE = "on_receive"
CONF_ON_BROADCAST = "on_broadcast"
CONF_ON_RAW_DATA = "on_raw_data"

CONF_ON_SUCCEED = "on_succeed"
CONF_ON_FAILED = "on_failed"
CONF_ON_TIMEOUT = "on_timeout"

ESPNowTriggers = espnow_ns.enum("ESPNowTriggers")
ALLOWED_TRIGGER_LIST = {
    CONF_ON_RECEIVE: ESPNowTriggers.TRIGGER_ON_RECEIVE,
    CONF_ON_BROADCAST: ESPNowTriggers.TRIGGER_ON_BROADCAST,
    CONF_ON_SUCCEED: ESPNowTriggers.TRIGGER_ON_SUCCEED,
    CONF_ON_FAILED: ESPNowTriggers.TRIGGER_ON_FAILED,
    CONF_ON_TIMEOUT: ESPNowTriggers.TRIGGER_ON_TIMEOUT,
}

CONF_APP_MODE = "app_mode"
CONF_UNIVERSAL = "universal"
CONF_COLLECTOR = "collector"
CONF_PROVIDER = "provider"

ESPNowAppMode = espnow_ns.enum("ESPNowAppMode")
ENUM_MODE = {
    CONF_UNIVERSAL: ESPNowAppMode.PM_UNIVERSAL,
    CONF_COLLECTOR: ESPNowAppMode.PM_COLLECTOR,
    CONF_PROVIDER: ESPNowAppMode.PM_PROVIDER,
}


validate_command = cv.Range(min=1, max=250)
validate_channel = cv.int_range(1, 14)
validate_app = cv.Any(cv.int_range(0x0001, 0xFFFF), "default")


def set_core_data(config):
    CORE.data[CONF_ESPNOW] = {}
    CORE.data[CONF_ESPNOW]["default_app_id"] = 0x5358
    CORE.data[CONF_ESPNOW]["default_peer"] = 0
    return config


def validate_unque_app_ids(config):
    idlist = [entry[CONF_APP_ID] for entry in config]
    if len(idlist) != len(set(idlist)):
        duplicates = ", ".join({hex(id) for id in idlist if idlist.count(id) != 1})
        raise cv.Invalid(f"Duplicated app ID's found in config: {duplicates}")
    return config


def espnow_hex(mac_address):
    it = reversed(mac_address.parts)
    num = "".join(f"{part:02X}" for part in it)
    return cg.RawExpression(f"0x{num}ULL")


DEFINE_PEER_CONFIG = cv.maybe_simple_value(
    {
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_DEFAULT): cv.boolean,
    },
    key=CONF_MAC_ADDRESS,
)

DEFAULT_TRIGGER_AUTOMATION_SCHEMA = automation.validate_automation(
    {
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowDefaultTrigger),
        cv.Optional(CONF_COMMAND): validate_command,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESPNowComponent),
        cv.Optional(CONF_WIFI_CHANNEL): validate_channel,
        cv.Optional(CONF_AUTO_ADD_PEER, default=False): cv.boolean,
        cv.Optional(CONF_WAIT_FOR_ACK, default=True): cv.boolean,
        cv.Optional(
            CONF_CONFORMATION_TIMEOUT, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ATTEMPTS, default=1): cv.int_range(min=1, max=10),
        cv.Optional(CONF_PREDEFINED_PEERS): cv.ensure_list(DEFINE_PEER_CONFIG),
        cv.Optional(CONF_ON_NEW_PEER): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_RAW_DATA): automation.validate_automation(single=True),
        cv.Optional(CONF_CUSTOM_APPS): cv.All(
            cv.ensure_list(
                cv.Schema(
                    {
                        cv.Optional(CONF_APP_ID, default=CONF_DEFAULT): validate_app,
                        cv.Optional(CONF_DEFAULT): cv.boolean,
                        cv.Optional(CONF_ON_RECEIVE): DEFAULT_TRIGGER_AUTOMATION_SCHEMA,
                        cv.Optional(
                            CONF_ON_BROADCAST
                        ): DEFAULT_TRIGGER_AUTOMATION_SCHEMA,
                        cv.Optional(CONF_ON_SUCCEED): DEFAULT_TRIGGER_AUTOMATION_SCHEMA,
                        cv.Optional(CONF_ON_FAILED): DEFAULT_TRIGGER_AUTOMATION_SCHEMA,
                        cv.Optional(CONF_ON_TIMEOUT): DEFAULT_TRIGGER_AUTOMATION_SCHEMA,
                    }
                ),
            ),
            validate_unque_app_ids,
        ),
    },
    cv.only_on_esp32,
    set_core_data,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if core.CORE.using_arduino:
        cg.add_library("WiFi", None)

    cg.add_define("USE_ESPNOW")
    if CONF_WIFI_CHANNEL in config:
        cg.add(var.set_wifi_channel(config[CONF_WIFI_CHANNEL]))

    cg.add(var.set_auto_add_peer(config[CONF_AUTO_ADD_PEER]))
    cg.add(var.set_wait_for_ack(config[CONF_WAIT_FOR_ACK]))
    cg.add(var.set_conformation_timeout(config[CONF_CONFORMATION_TIMEOUT]))
    cg.add(var.set_attempts(config[CONF_ATTEMPTS]))
    def_peer = 0
    first_peer = 0
    for conf in config.get(CONF_PREDEFINED_PEERS, []):
        mac = espnow_hex(conf.get(CONF_MAC_ADDRESS))
        if config.get(CONF_DEFAULT, False):
            def_peer = mac
        if first_peer == 0:
            first_peer = mac

        cg.add(var.add_peer(mac))
    if def_peer:
        CORE.data[CONF_ESPNOW]["default_peer"] = def_peer
    else:
        CORE.data[CONF_ESPNOW]["default_peer"] = first_peer

    if CONF_ON_NEW_PEER in config:
        await automation.build_automation(
            var.get_new_peer_trigger(),
            [(cg.std_shared_ptr.template(ESPNowPacket), "packet")],
            config[CONF_ON_NEW_PEER],
        )
    if CONF_ON_RAW_DATA in config:
        await automation.build_automation(
            var.get_raw_data_trigger(),
            [(cg.std_shared_ptr.template(ESPNowPacket), "packet")],
            config[CONF_ON_RAW_DATA],
        )

    for key, code in ALLOWED_TRIGGER_LIST.items():
        for conf in config.get(key, []):
            cmd = conf.get(CONF_COMMAND, 0)
            event = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var, 0, 0, code)
            await automation.build_automation(
                event, [(cg.std_shared_ptr.template(ESPNowPacket), "packet")], conf
            )
    def_app_id = 0
    first_app_id = 0
    if apps := config.get(CONF_CUSTOM_APPS, []):
        for app in apps:
            app_id = app.get(CONF_APP_ID)
            if app_id == "default":
                app_id = CORE.data[CONF_ESPNOW]["default_app_id"]
                def_app_id = app_id

            if app.get(CONF_DEFAULT, False) and def_app_id == 0:
                def_app_id = app_id
            if first_app_id == 0:
                first_app_id = app_id

            for key, code in ALLOWED_TRIGGER_LIST.items():
                for conf in app.get(key, []):
                    cmd = conf.get(CONF_COMMAND, 0)
                    event = cg.new_Pvariable(
                        conf[CONF_TRIGGER_ID], var, app_id, cmd, code
                    )
                    await automation.build_automation(
                        event,
                        [(cg.std_shared_ptr.template(ESPNowPacket), "packet")],
                        conf,
                    )
    if def_app_id:
        CORE.data[CONF_ESPNOW]["default_app_id"] = def_app_id
    else:
        CORE.data[CONF_ESPNOW]["default_app_id"] = first_app_id
    cg.add(var.set_default_app_id(CORE.data[CONF_ESPNOW]["default_app_id"]))


ESPNOW_APP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESPNOW): cv.use_id(ESPNowComponent),
        cv.Optional(CONF_APP_MODE): cv.enum(ENUM_MODE, string=True),
    },
    cv.only_on_esp32,
)


async def register_app(var, config):
    now = await cg.get_variable(config[CONF_ESPNOW])
    cg.add(now.register_app(var))
    if CONF_APP_MODE in config:
        cg.add(var.set_app_mode(config[CONF_APP_MODE]))


def _final_validate(config):
    full_config = fv.full_config.get()
    if CONF_WIFI_CHANNEL in config and CONF_WIFI in full_config:
        raise cv.Invalid(
            f"When you have {CONF_WIFI} configured, You can not set the {CONF_WIFI_CHANNEL} variable."
        )
    if CONF_WIFI_CHANNEL not in config and CONF_WIFI not in full_config:
        raise cv.Invalid(
            f"When you don't use the {CONF_WIFI} component, You need to set the {CONF_WIFI_CHANNEL} variable."
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# ========================================== A C T I O N S ================================================


def validate_peer(value):
    if value == "default":
        return value
    if isinstance(value, cv.Lambda):
        return cv.returning_lambda(value)
    return cv.mac_address(value)


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        f"'{CONF_PAYLOAD}' must either be a string wrapped in quotes or a list of bytes"
    )


async def register_peer(var, config, args):
    peer = config.get(CONF_MAC_ADDRESS, 0xFFFFFFFFFFFF)
    if peer == "default":
        peer = CORE.data[CONF_ESPNOW]["default_peer"]
    if isinstance(peer, core.MACAddress):
        peer = espnow_hex(peer)

    template_ = await cg.templatable(peer, args, cg.uint64)
    cg.add(var.set_mac_address(template_))


SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ESPNowComponent),
        cv.Optional(CONF_APP_ID, default="default"): validate_app,
        cv.Optional(CONF_COMMAND, default=0): validate_command,
        cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
        cv.Optional(CONF_ON_SUCCEED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowDefaultTrigger),
            },
            single=True,
        ),
        cv.Optional(CONF_ON_FAILED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowDefaultTrigger),
            },
            single=True,
        ),
        cv.Optional(CONF_ON_TIMEOUT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowDefaultTrigger),
            },
            single=True,
        ),
    }
)


@automation.register_action(
    "espnow.send.broadcast",
    SendAction,
    cv.maybe_simple_value(
        SEND_SCHEMA.extend(
            {
                cv.Optional(CONF_MAC_ADDRESS, default=0xFFFFFFFFFFFF): cv.uint64_t,
            }
        ),
        key=CONF_PAYLOAD,
    ),
)
@automation.register_action(
    "espnow.send.multicast",
    SendAction,
    cv.maybe_simple_value(
        SEND_SCHEMA.extend(
            {
                cv.Optional(CONF_MAC_ADDRESS, default=0xFFFFFFFFFFFE): cv.uint64_t,
            }
        ),
        key=CONF_PAYLOAD,
    ),
)
@automation.register_action(
    "espnow.send",
    SendAction,
    SEND_SCHEMA.extend(
        {
            cv.Optional(CONF_MAC_ADDRESS, default="default"): validate_peer,
            cv.Optional(CONF_DONT_WAIT): cv.boolean,
        }
    ),
)
async def send_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    await register_peer(var, config, args)

    command = config.get(CONF_COMMAND)
    cg.add(var.set_command(command))

    app_id = config.get(CONF_APP_ID)
    if app_id == "default":
        app_id = CORE.data[CONF_ESPNOW]["default_app_id"]
    cg.add(var.set_app_id(app_id))

    data = config.get(CONF_PAYLOAD, [])
    if isinstance(data, bytes):
        data = list(data)

    vec_ = cg.std_vector.template(cg.uint8)
    templ = await cg.templatable(data, args, vec_, vec_)
    cg.add(var.set_payload(templ))

    if config.get(CONF_DONT_WAIT, False):
        cg.add(var.set_dont_wait_flag())

    for trigger_name, event_code in ALLOWED_TRIGGER_LIST.items():
        for conf in config.get(trigger_name, []):
            trigger = cg.new_Pvariable(
                conf[CONF_TRIGGER_ID], var, app_id, command, event_code
            )
            await automation.build_automation(
                trigger, [(cg.std_shared_ptr.template(ESPNowPacket), "packet")], conf
            )

    return var


@automation.register_action(
    "espnow.peer.add",
    NewPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): validate_peer,
            cv.Optional(CONF_WIFI_CHANNEL): cv.invalid(
                CONF_WIFI_CHANNEL + " No longer in use."
            ),
        },
        key=CONF_MAC_ADDRESS,
    ),
)
@automation.register_action(
    "espnow.peer.del",
    DelPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): validate_peer,
        },
        key=CONF_MAC_ADDRESS,
    ),
)
async def peer_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    await register_peer(var, config, args)

    return var


@automation.register_action(
    "espnow.channel.set",
    SetChannelAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_WIFI_CHANNEL): cv.templatable(validate_channel),
        },
        key=CONF_WIFI_CHANNEL,
    ),
)
async def channel_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_WIFI_CHANNEL], args, cg.uint8)
    cg.add(var.set_channel(template_))
    return var
