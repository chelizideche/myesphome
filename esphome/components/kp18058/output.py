import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_CHANNEL
from . import KP18058

KP18058_ns = cg.esphome_ns.namespace("kp18058")
DriverOutput = KP18058_ns.class_("kp18058_output", output.FloatOutput)

CONF_KP18058_ID = "kp18058_id"

# Storage for outputs, to be validated at the end
_output_registry = {}

def validate_unique_channels(config):

    kp18058_id = str(config[CONF_KP18058_ID])
    channel = config[CONF_CHANNEL]

    if kp18058_id not in _output_registry:
        _output_registry[kp18058_id] = set()

    if channel in _output_registry[kp18058_id]:
        raise cv.Invalid(f"Channel {channel} is already used for kp18058 component with id {kp18058_id}. Each output must have a unique channel.")

    _output_registry[kp18058_id].add(channel)

    return config

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA
    .extend({
          cv.Required(CONF_ID): cv.declare_id(DriverOutput),
          cv.GenerateID(CONF_KP18058_ID): cv.use_id(KP18058),
          cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=5),
    }),
    validate_unique_channels,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_KP18058_ID])

    cg.add(var.set_parent(parent))
    cg.add(parent.set_output_channel(config[CONF_CHANNEL], var))

    await output.register_output(var, config)
