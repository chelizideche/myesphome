import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    UNIT_AMPERE,
    UNIT_VOLT,
    ENTITY_CATEGORY_CONFIG,
)

from .. import atm90e32_ns
from ..sensor import ATM90E32Component

ATM90E32Number = atm90e32_ns.class_(
    "ATM90E32Number", number.Number, cg.Parented.template(ATM90E32Component)
)

CONF_REFERENCE_CURRENT = "reference_current"
CONF_REFERENCE_VOLTAGE = "reference_voltage"
CONF_PHASE_A = "phase_a"
CONF_PHASE_B = "phase_b"
CONF_PHASE_C = "phase_c"
PHASE_KEYS = [CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]

def reference_voltage_schema(): 
    return cv.All(
        cv.Schema({
            cv.Required(CONF_NAME): cv.string,
            cv.Optional("mode", default="box"): cv.string,
            cv.Optional("optimistic", default=True): cv.boolean,
            cv.Optional("min_value", default=100.0): cv.float_,
            cv.Optional("max_value", default=260.0): cv.float_,
            cv.Optional("step", default=0.1): cv.float_,
        }).extend(
            number.number_schema(
                class_=ATM90E32Number,
                unit_of_measurement=UNIT_VOLT,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:power-plug"
            )
        )
    )

def reference_current_schema(): 
    return cv.All(
        cv.Schema({
            cv.Required(CONF_NAME): cv.string,
            cv.Optional("mode", default="box"): cv.string,
            cv.Optional("optimistic", default=True): cv.boolean,
            cv.Optional("min_value", default=1.0): cv.float_,
            cv.Optional("max_value", default=200.0): cv.float_,
            cv.Optional("step", default=0.1): cv.float_,
        }).extend(
            number.number_schema(
                class_=ATM90E32Number,
                unit_of_measurement=UNIT_AMPERE,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:home-lightning-bolt"
            )
        )
    )

REFERENCE_VOLTAGE_SCHEMA = cv.Schema({
    cv.Optional(CONF_PHASE_A): reference_voltage_schema(),
    cv.Optional(CONF_PHASE_B): reference_voltage_schema(),
    cv.Optional(CONF_PHASE_C): reference_voltage_schema(),
})

REFERENCE_CURRENT_SCHEMA = cv.Schema({
    cv.Optional(CONF_PHASE_A): reference_current_schema(),
    cv.Optional(CONF_PHASE_B): reference_current_schema(),
    cv.Optional(CONF_PHASE_C): reference_current_schema(),
})

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(ATM90E32Component),
    cv.Optional(CONF_REFERENCE_VOLTAGE): REFERENCE_VOLTAGE_SCHEMA,
    cv.Optional(CONF_REFERENCE_CURRENT): REFERENCE_CURRENT_SCHEMA,
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_REFERENCE_VOLTAGE in config:
        voltage_cfg = config[CONF_REFERENCE_VOLTAGE]
        voltage_objs = [None, None, None]

        for i, key in enumerate(PHASE_KEYS):
            if key in voltage_cfg:
                validated = reference_voltage_schema()(voltage_cfg[key])
                obj = await number.new_number(
                    validated,
                    min_value=validated["min_value"],
                    max_value=validated["max_value"],
                    step=validated["step"],
                )
                await cg.register_parented(obj, parent)
                voltage_objs[i] = obj

        # Inherit from A → B/C if only A defined
        if voltage_objs[0] is not None:
            for i in range(3):
                if voltage_objs[i] is None:
                    voltage_objs[i] = voltage_objs[0]

        for i in range(3):
            if voltage_objs[i] is not None:
                cg.add(parent.set_reference_voltage(i, voltage_objs[i]))


    if CONF_REFERENCE_CURRENT in config:
        current_cfg = config[CONF_REFERENCE_CURRENT]
        for i, key in enumerate(PHASE_KEYS):
            if key in current_cfg:
                validated = reference_current_schema()(current_cfg[key])
                obj = await number.new_number(
                    validated,
                    min_value=validated["min_value"],
                    max_value=validated["max_value"],
                    step=validated["step"],
                )
                await cg.register_parented(obj, parent)
                cg.add(parent.set_reference_current(i, obj))