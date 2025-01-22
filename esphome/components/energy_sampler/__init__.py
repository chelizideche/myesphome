import esphome.codegen as cg

energy_sampler_ns = cg.esphome_ns.namespace("energy_sampler")

# Define the EnergySampler class
EnergySampler = energy_sampler_ns.class_("EnergySampler")

# Define the Phase enum
Phase = energy_sampler_ns.enum("Phase", {
    "PHASE_1": 0,  # Phase 1 (index 0)
    "PHASE_2": 1,  # Phase 2 (index 1)
    "PHASE_3": 2   # Phase 3 (index 2)
})

# Define the PhaseSampleData struct
PhaseSampleData = energy_sampler_ns.struct(
    "PhaseSampleData",
    fields={
        "voltage": cg.float_,  # Voltage in volts (V)
        "current": cg.float_   # Current in amperes (A)
    }
)

# Define the EnergySampleData struct
EnergySampleData = energy_sampler_ns.struct(
    "EnergySampleData",
    fields={
        "phases": cg.array(cg.template(PhaseSampleData), 3),  # Array of 3 PhaseSampleData structs
        "neutral_current": cg.float_  # Neutral current in amperes (A)
    }
)
