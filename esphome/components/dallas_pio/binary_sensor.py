import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_INVERTED, CONF_MODE, CONF_PIN

DEPENDENCIES = ["binary_sensor", "dallas_pio"]
AUTO_LOAD = ["binary_sensor"]

dallas_pio_ns = cg.esphome_ns.namespace("dallas_pio")
DallasPioBinarySensor = dallas_pio_ns.class_(
    "DallasPioBinarySensor",
    cg.PollingComponent,
    binary_sensor.BinarySensor,
)


def validate_mode(value):
    if value.get("input", False) is not True:
        raise cv.Invalid(
            "The 'input' field must always be 'true' for Dallas PIO binary sensors."
        )
    if value.get("output", False) is True:
        raise cv.Invalid("The 'output' field must be 'false' if 'input' is 'true'.")
    return value


CUSTOM_PIN_SCHEMA = cv.Schema(
    {
        cv.Required("number"): cv.one_of(
            "PIOA", "PIOB", "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7", upper=True
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_MODE, default={"input": True}): validate_mode,
    }
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        DallasPioBinarySensor,
    )
    .extend(
        {
            cv.Required(CONF_PIN): CUSTOM_PIN_SCHEMA,
            cv.Required("dallas_pio_id"): cv.use_id(dallas_pio_ns.class_("DallasPio")),
        }
    )
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    dallas_pio_ref = await cg.get_variable(config["dallas_pio_id"])
    cg.add(var.set_dallas_pio(dallas_pio_ref))

    # Extract pin configuration
    pin_config = config[CONF_PIN]
    pin_number = pin_config["number"]  # "PIOA" or "PIOB"
    pin_inverted = pin_config[CONF_INVERTED]
    pin_mode = pin_config[CONF_MODE]
    # C++ configuration association
    if pin_number == "PIOA":
        cg.add(var.set_pin(0x01))  # PIOA assiociated to 0x01
    elif pin_number == "PIOB":
        cg.add(var.set_pin(0x02))  # PIOB assiociated to 0x02
    elif pin_number.startswith("P"):
        pin_index = int(pin_number[1:])
        if 0 <= pin_index <= 7:
            cg.add(var.set_pin(1 + pin_index))
    cg.add(var.set_pin_inverted(pin_inverted))
    cg.add(var.set_pin_mode(pin_mode.get("input", True), pin_mode.get("output", False)))
    # cg.add(var.set_inverted(inverted))
