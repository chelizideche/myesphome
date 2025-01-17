import esphome.codegen as cg
from esphome.components import one_wire
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME

dallas_pio_ns = cg.esphome_ns.namespace("dallas_pio")
DallasPio = dallas_pio_ns.class_(
    "DallasPio",
    cg.Component,
    one_wire.OneWireDevice,
)

CONF_REFERENCE = "reference"
CONF_CRC = "crc"


def validate_crc_option(config_list):
    for conf in config_list:
        reference = conf.get("reference", "").upper()
        if not reference or reference == "DS2413":
            continue
        if reference == "DS2406" and "crc" not in conf:
            raise cv.Invalid("Option 'crc' is required when 'reference' is DS2406.")
        if reference != "DS2406" and "crc" in conf:
            raise cv.Invalid("Option 'crc' is not supported for this reference.")
    return config_list


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(DallasPio),
                cv.Optional(CONF_NAME): cv.string_strict,
                cv.Required(CONF_ADDRESS): cv.hex_uint64_t,
                cv.Optional(CONF_REFERENCE, default=""): cv.one_of(
                    "", "DS2413", "DS2406", "DS2408", upper=True
                ),
                cv.Optional(CONF_CRC, default=False): cv.boolean,
            }
        ).extend(one_wire.one_wire_device_schema())
    ),
    validate_crc_option,
)


async def to_code(config):
    for conf in config:
        var = cg.new_Pvariable(conf[CONF_ID])
        if CONF_NAME in conf:
            cg.add(var.set_name(conf[CONF_NAME]))
        cg.add(var.set_address(conf[CONF_ADDRESS]))
        if CONF_REFERENCE in conf:
            cg.add(var.set_reference(conf[CONF_REFERENCE]))
        if CONF_CRC in conf and conf[CONF_REFERENCE] == "DS2406":
            cg.add(var.set_crc_enabled(conf[CONF_CRC]))
        if CONF_ID in conf:
            cg.add(var.set_id(conf[CONF_ID].id))
        if "one_wire_id" in conf:
            cg.add(var.set_one_wire_id(conf["one_wire_id"].id))
        await cg.register_component(var, config)
        await one_wire.register_one_wire_device(var, conf)
