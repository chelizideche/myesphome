
from esphome.components import climate, tuya
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_SWITCH_DATAPOINT,
    CONF_SUPPORTS_HEAT,
    CONF_HEATER
)
from tuya import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jshsltr"]

CONF_ACTIVE_STATE = "active_state"
CONF_DATAPOINT = "datapoint"
CONF_HEATING_VALUE = "heating_value"
CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TEMPERATURE_MULTIPLIER = "temperature_multiplier"
CONF_CURRENT_TEMPERATURE_MULTIPLIER = "current_temperature_multiplier"
CONF_TARGET_TEMPERATURE_MULTIPLIER = "target_temperature_multiplier"
CONF_ECO_MODE = "eco_mode"
CONF_E1_VALUE = "e1_value"
CONF_E2_VALUE = "e2_value"
CONF_HEAT_LEVEL = "heat_level"
CONF_P1_VALUE = "p1_value"
CONF_P2_VALUE = "p2_value"
CONF_P3_VALUE = "p3_value"
CONF_P4_VALUE = "p4_value"
CONF_REPORTS_FAHRENHEIT = "reports_fahrenheit"


TuyaPellet = tuya_ns.class_("TuyaPellet", climate.Climate, cg.Component, )

def validate_temperature_multipliers(value):
    if CONF_TEMPERATURE_MULTIPLIER in value:
        if (
            CONF_CURRENT_TEMPERATURE_MULTIPLIER in value
            or CONF_TARGET_TEMPERATURE_MULTIPLIER in value
        ):
            raise cv.Invalid(
                f"Cannot have {CONF_TEMPERATURE_MULTIPLIER} at the same time as "
                f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER} and "
                f"{CONF_TARGET_TEMPERATURE_MULTIPLIER}"
            )
    if (
        CONF_CURRENT_TEMPERATURE_MULTIPLIER in value
        and CONF_TARGET_TEMPERATURE_MULTIPLIER not in value
    ):
        raise cv.Invalid(
            f"{CONF_TARGET_TEMPERATURE_MULTIPLIER} required if using "
            f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER}"
        )
    if (
        CONF_TARGET_TEMPERATURE_MULTIPLIER in value
        and CONF_CURRENT_TEMPERATURE_MULTIPLIER not in value
    ):
        raise cv.Invalid(
            f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER} required if using "
            f"{CONF_TARGET_TEMPERATURE_MULTIPLIER}"
        )
    keys = (
        CONF_TEMPERATURE_MULTIPLIER,
        CONF_CURRENT_TEMPERATURE_MULTIPLIER,
        CONF_TARGET_TEMPERATURE_MULTIPLIER,
    )
    if all(multiplier not in value for multiplier in keys):
        value[CONF_TEMPERATURE_MULTIPLIER] = 1.0
    return value


ACTIVE_STATES = cv.Schema(
    {
        cv.Required(CONF_DATAPOINT): cv.uint8_t,
        cv.Optional(CONF_HEATING_VALUE, default=1): cv.uint8_t,
    },
)

HEATER_MODES = cv.Schema(
    {
        cv.Optional(CONF_ECO_MODE): {
            cv.Required(CONF_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_E1_VALUE): cv.uint8_t,
            cv.Optional(CONF_E2_VALUE): cv.uint8_t,
        },
        cv.Optional(CONF_HEAT_LEVEL): {
            cv.Required(CONF_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_P1_VALUE): cv.uint8_t,
            cv.Optional(CONF_P2_VALUE): cv.uint8_t,
            cv.Optional(CONF_P3_VALUE): cv.uint8_t,
            cv.Optional(CONF_P4_VALUE): cv.uint8_t,
        },
    },
)

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TuyaPellet),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
            cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_ACTIVE_STATE): ACTIVE_STATES,
            cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_CURRENT_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_TARGET_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_REPORTS_FAHRENHEIT, default=False): cv.boolean,
            cv.Optional(CONF_HEATER): HEATER_MODES,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_TARGET_TEMPERATURE_DATAPOINT, CONF_SWITCH_DATAPOINT),
    validate_temperature_multipliers,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    if switch_datapoint := config.get(CONF_SWITCH_DATAPOINT):
        cg.add(var.set_switch_id(switch_datapoint))

    if active_state_config := config.get(CONF_ACTIVE_STATE):
        cg.add(var.set_active_state_id(active_state_config.get(CONF_DATAPOINT)))
        if (heating_value := active_state_config.get(CONF_HEATING_VALUE)) is not None:
            cg.add(var.set_active_state_heating_value(heating_value))

    if target_temperature_datapoint := config.get(CONF_TARGET_TEMPERATURE_DATAPOINT):
        cg.add(var.set_target_temperature_id(target_temperature_datapoint))
    if current_temperature_datapoint := config.get(CONF_CURRENT_TEMPERATURE_DATAPOINT):
        cg.add(var.set_current_temperature_id(current_temperature_datapoint))

    if heater_config := config.get(CONF_HEATER, {}):
        if eco_mode_config := heater_config.get(CONF_ECO_MODE, {}):
            cg.add(var.set_eco_mode_id(eco_mode_config.get(CONF_DATAPOINT)))
            if (e1_value := eco_mode_config.get(CONF_E1_VALUE)) is not None:
                cg.add(var.set_e1_value_(e1_value))
            if (e2_value := eco_mode_config.get(CONF_E2_VALUE)) is not None:
                cg.add(var.set_e2_value_(e2_value))
        if heat_level_config := heater_config.get(CONF_HEAT_LEVEL, {}):
            cg.add(var.set_heat_level_id(heat_level_config.get(CONF_DATAPOINT)))
            if (p1_value := heat_level_config.get(CONF_P1_VALUE)) is not None:
                cg.add(var.set_p1_value_(p1_value))
            if (p2_value := eco_mode_config.get(CONF_P2_VALUE)) is not None:
                cg.add(var.set_p2_value_(p2_value))
            if (p3_value := eco_mode_config.get(CONF_P3_VALUE)) is not None:
                cg.add(var.set_p3_value_(p3_value))
            if (p4_value := eco_mode_config.get(CONF_P4_VALUE)) is not None:
                cg.add(var.set_p4_value_(p4_value))

    if temperature_multiplier := config.get(CONF_TEMPERATURE_MULTIPLIER):
        cg.add(var.set_target_temperature_multiplier(temperature_multiplier))
        cg.add(var.set_current_temperature_multiplier(temperature_multiplier))
    else:
        if current_temperature_multiplier := config.get(
            CONF_CURRENT_TEMPERATURE_MULTIPLIER
        ):
            cg.add(
                var.set_current_temperature_multiplier(current_temperature_multiplier)
            )
        if target_temperature_multiplier := config.get(
            CONF_TARGET_TEMPERATURE_MULTIPLIER
        ):
            cg.add(var.set_target_temperature_multiplier(target_temperature_multiplier))

    if config[CONF_REPORTS_FAHRENHEIT]:
        cg.add(var.set_reports_fahrenheit())
