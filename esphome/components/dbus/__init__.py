# requires dbus-1-devel

import subprocess

from esphome import automation

# from esphome.automation import LambdaAction
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, PLATFORM_HOST
from esphome.coroutine import coroutine_with_priority

CODEOWNERS = ["@ferbar"]

CONF_DBUS_SYSTEM = "dbus_system"
CONF_DBUS_DESTINATION = "dbus_destination"
CONF_DBUS_PATH = "dbus_path"
CONF_DBUS_INTERFACE = "dbus_interface"
CONF_DBUS_METHOD = "dbus_method"
CONF_DBUS_ARGS = "dbus_args"

dbus_ns = cg.esphome_ns.namespace("dbus")
DBus = dbus_ns.class_("DBus", cg.Component)

DBusSendAction = dbus_ns.class_(
    "DBusSendAction", automation.Action, cg.Parented.template(DBus)
)


# Actions
@automation.register_action(
    "dbus.send",
    DBusSendAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DBus),
            cv.Optional(CONF_DBUS_SYSTEM, default=False): cv.templatable(cv.boolean),
            cv.Required(CONF_DBUS_DESTINATION): cv.templatable(cv.string),
            cv.Required(CONF_DBUS_PATH): cv.templatable(cv.string),
            cv.Required(CONF_DBUS_INTERFACE): cv.templatable(cv.string),
            cv.Required(CONF_DBUS_METHOD): cv.templatable(cv.string),
            # workaround: "bool:True" "bool:False" to set a boolean argument
            cv.Optional(CONF_DBUS_ARGS, default={}): cv.ensure_list(cv.string),
        },
    ),
)
async def dbus_send_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    dbus_system = await cg.templatable(config[CONF_DBUS_SYSTEM], args, bool)
    cg.add(var.set_dbus_system(dbus_system))
    dbus_destination = await cg.templatable(
        config[CONF_DBUS_DESTINATION], args, cg.std_string
    )
    cg.add(var.set_dbus_destination(dbus_destination))
    dbus_path = await cg.templatable(config[CONF_DBUS_PATH], args, cg.std_string)
    cg.add(var.set_dbus_path(dbus_path))
    dbus_interface = await cg.templatable(
        config[CONF_DBUS_INTERFACE], args, cg.std_string
    )
    cg.add(var.set_dbus_interface(dbus_interface))
    dbus_method = await cg.templatable(config[CONF_DBUS_METHOD], args, cg.std_string)
    cg.add(var.set_dbus_method(dbus_method))
    dbus_args = await cg.templatable(config[CONF_DBUS_ARGS], args, cg.std_string)
    cg.add(var.set_dbus_args(dbus_args))
    return var


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DBus),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_HOST]),
)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(dbus_ns.using)
    cg.add_define("USE_DBUS")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cFlags = subprocess.run(
        ["pkg-config", "--cflags", "dbus-1"], capture_output=True, check=False
    )
    if cFlags.returncode != 0:
        raise ValueError("Error: pkg-config dbus-1 failed!")
    linkerFlags = subprocess.run(
        ["pkg-config", "--libs", "dbus-1"], capture_output=True, check=False
    )
    if linkerFlags.returncode != 0:
        raise ValueError("Error: pkg-config dbus-1 failed!")
    # print("dbus cflags: ", cFlags.stdout, " linker: ", linkerFlags.stdout)
    cg.add_build_flag(cFlags.stdout.decode("utf-8"))
    cg.add_build_flag(linkerFlags.stdout.decode("utf-8"))
