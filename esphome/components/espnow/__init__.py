from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_COMMAND, CONF_ID, CONF_PAYLOAD, CONF_TRIGGER_ID
from esphome.core import CORE

CODEOWNERS = ["@nielsnl68", "@jesserockz"]

espnow_ns = cg.esphome_ns.namespace("espnow")
ESPNowComponent = espnow_ns.class_("ESPNowComponent", cg.Component)
ESPNowListener = espnow_ns.class_("ESPNowListener")

ESPNowPacket = espnow_ns.class_("ESPNowPacket")
ESPNowPeer = espnow_ns.class_("Peer")

ESPNowPacketConst = ESPNowPacket.operator("const")


ESPNowInterface = espnow_ns.class_(
    "ESPNowInterface", cg.Component, cg.Parented.template(ESPNowComponent)
)

ESPNowSentTrigger = espnow_ns.class_("ESPNowSentTrigger", automation.Trigger.template())
ESPNowReceiveTrigger = espnow_ns.class_(
    "ESPNowReceiveTrigger", automation.Trigger.template()
)
ESPNowNewPeerTrigger = espnow_ns.class_(
    "ESPNowNewPeerTrigger", automation.Trigger.template()
)
ESPNowBroadcaseTrigger = espnow_ns.class_(
    "ESPNowBroadcaseTrigger", automation.Trigger.template()
)
SendAction = espnow_ns.class_("SendAction", automation.Action)
NewPeerAction = espnow_ns.class_("NewPeerAction", automation.Action)
DelPeerAction = espnow_ns.class_("DelPeerAction", automation.Action)

CONF_AUTO_ADD_PEER = "auto_add_peer"
CONF_CONFORMATION_TIMEOUT = "conformation_timeout"
CONF_ESPNOW = "espnow"
CONF_RETRIES = "retries"
CONF_ON_RECEIVE = "on_receive"
CONF_ON_BROADCAST = "on_broadcast"
CONF_ON_SENT = "on_sent"
CONF_ON_NEW_PEER = "on_new_peer"
CONF_PEER = "peer"
CONF_PEERS = "peers"
CONF_USE_SENT_CHECK = "use_sent_check"
CONF_WIFI_CHANNEL = "wifi_channel"


CONF_MAC_CHARS = "0123456789-AbCdEfGhIjKlMnOpQrStUvWxYz+aBcDeFgHiJkLmNoPqRsTuVwXyZ"


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


def convert_mac_address(value):
    parts = value.split(":")
    if len(parts) != 6:
        raise cv.Invalid("MAC Address must consist of 6 : (colon) separated parts")
    parts_int = 0
    if any(len(part) != 2 for part in parts):
        raise cv.Invalid("MAC Address must be format XX:XX:XX:XX:XX:XX")
    for part in parts:
        try:
            parts_int = (parts_int << 8) + int(part, 16)
        except ValueError:
            # pylint: disable=raise-missing-from
            raise cv.Invalid(
                "MAC Address parts must be hexadecimal values from 00 to FF"
            )
    return parts_int


def validate_peer(value):
    if isinstance(value, (int)):
        return value

    if value.find(":") != -1:
        return convert_mac_address(value)

    if len(value) == 8:
        value = cv.string_strict(value)

        mac = 0
        for x in value:
            n = CONF_MAC_CHARS.find(x)
            if n == -1:
                raise cv.Invalid(f"peer code is invalid. ({value}|{x})")
            mac = (mac << 6) + n
        return mac

    raise cv.Invalid(
        f"peer code '{value}' needs to be 8 characters width, or a valid Mac address or a hexidacimal value of 12 chars width starting with '0x'"
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESPNowComponent),
        cv.Optional(CONF_WIFI_CHANNEL, default=0): cv.int_range(0, 14),
        cv.Optional(CONF_AUTO_ADD_PEER, default=False): cv.boolean,
        cv.Optional(CONF_USE_SENT_CHECK, default=True): cv.boolean,
        cv.Optional(
            CONF_CONFORMATION_TIMEOUT, default="5000ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RETRIES, default=5): cv.int_range(min=1, max=10),
        cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowReceiveTrigger),
                cv.Optional(CONF_COMMAND): cv.Range(min=16, max=255),
            }
        ),
        cv.Optional(CONF_ON_BROADCAST): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowBroadcaseTrigger),
                cv.Optional(CONF_COMMAND): cv.Range(min=0, max=255),
            }
        ),
        cv.Optional(CONF_ON_SENT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowSentTrigger),
                cv.Optional(CONF_COMMAND): cv.Range(min=0, max=255),
            }
        ),
        cv.Optional(CONF_ON_NEW_PEER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowNewPeerTrigger),
                cv.Optional(CONF_COMMAND): cv.Range(min=16, max=255),
            }
        ),
        cv.Optional(CONF_PEERS): cv.ensure_list(validate_peer),
    },
    cv.only_on_esp32,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CORE.using_arduino:
        cg.add_library("WiFi", None)

    cg.add_define("USE_ESPNOW")

    cg.add(var.set_wifi_channel(config[CONF_WIFI_CHANNEL]))
    cg.add(var.set_auto_add_peer(config[CONF_AUTO_ADD_PEER]))
    cg.add(var.set_use_sent_check(config[CONF_USE_SENT_CHECK]))
    cg.add(var.set_conformation_timeout(config[CONF_CONFORMATION_TIMEOUT]))
    cg.add(var.set_retries(config[CONF_RETRIES]))

    for conf in config.get(CONF_PEERS, []):
        cg.add(var.add_peer(conf))

    for conf in config.get(CONF_ON_SENT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger,
            [(ESPNowPacketConst, "packet"), (bool, "status")],
            conf,
        )

    for conf in config.get(CONF_ON_RECEIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger, [(ESPNowPacketConst, "packet")], conf
        )

    for conf in config.get(CONF_ON_BROADCAST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger, [(ESPNowPacketConst, "packet")], conf
        )

    for conf in config.get(CONF_ON_NEW_PEER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger, [(ESPNowPacketConst, "packet")], conf
        )


PROTOCOL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESPNOW): cv.use_id(ESPNowComponent),
    },
    cv.only_on_esp32,
).extend(cv.COMPONENT_SCHEMA)


async def register_protocol(var, config):
    now = await cg.get_variable(config[CONF_ESPNOW])
    cg.add(now.register_protocol(var))


@automation.register_action(
    "espnow.broatcast",
    SendAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
            cv.Optional(CONF_COMMAND): cv.templatable(cv.Range(min=16, max=255)),
        },
        key=CONF_PAYLOAD,
    ),
)
@automation.register_action(
    "espnow.send",
    SendAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_PEER): cv.templatable(validate_peer),
            cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
            cv.Optional(CONF_COMMAND, default=0): cv.templatable(
                cv.Range(min=0, max=255)
            ),
        }
    ),
)
async def send_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    peer = config.get(CONF_PEER, 0xFFFFFFFFFFFF)
    template_ = await cg.templatable(peer, args, cg.uint64)
    cg.add(var.set_peer(template_))

    command = config.get(CONF_COMMAND, 0)
    template_ = await cg.templatable(command, args, cg.uint8)
    cg.add(var.set_command(template_))

    data = config.get(CONF_PAYLOAD, [])
    if isinstance(data, bytes):
        data = list(data)

    vec_ = cg.std_vector.template(cg.uint8)
    templ = await cg.templatable(data, args, vec_, vec_)
    cg.add(var.set_payload(templ))

    return var


@automation.register_action(
    "espnow.peer.new",
    NewPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_PEER): cv.templatable(validate_peer),
        },
        key=CONF_PEER,
    ),
)
@automation.register_action(
    "espnow.peer.del",
    DelPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_PEER): cv.templatable(validate_peer),
        },
        key=CONF_PEER,
    ),
)
async def add_del_peer_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_PEER], args, cg.uint64)
    cg.add(var.set_peer(template_))
    return var
