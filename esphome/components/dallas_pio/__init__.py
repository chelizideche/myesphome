from .dallas_pio import CONFIG_SCHEMA, DallasPio, to_code

CODEOWNERS = ["@tdy91"]

DEPENDENCIES = ["one_wire", "binary_sensor", "switch"]
AUTO_LOAD = ["binary_sensor", "switch"]

__all__ = ["CONFIG_SCHEMA", "DallasPio", "to_code"]
