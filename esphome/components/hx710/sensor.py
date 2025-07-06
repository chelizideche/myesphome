from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_MODE,
    CONF_REFERENCE_VOLTAGE,
    DEVICE_CLASS_VOLTAGE,
    ICON_GAUGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

AUTO_LOAD = ["voltage_sampler"]

hx710_ns = cg.esphome_ns.namespace("hx710")
HX710Sensor = hx710_ns.class_(
    "HX710Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    voltage_sampler.VoltageSampler,
)

CONF_DOUT_PIN = "dout_pin"


HX710Mode = hx710_ns.enum("HX710Mode")
MODES = {
    1: HX710Mode.HX710_DIFFERENTIAL_INPUT_10HZ,
    2: HX710Mode.HX710_OTHER_INPUT_40HZ,
    3: HX710Mode.HX710_DIFFERENTIAL_INPUT_40HZ,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        HX710Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_GAUGE,
    )
    .extend(
        {
            cv.Required(CONF_DOUT_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="0.0V"): cv.voltage,
            cv.Optional(CONF_MODE, default=1): cv.enum(MODES, int=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    dout_pin = await cg.gpio_pin_expression(config[CONF_DOUT_PIN])
    cg.add(var.set_dout_pin(dout_pin))
    sck_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
    cg.add(var.set_sck_pin(sck_pin))
    cg.add(var.set_gain(config[CONF_MODE]))
