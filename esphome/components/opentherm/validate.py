from collections.abc import Callable
from typing import Any

from voluptuous import Schema

import esphome.config_validation as cv
import esphome.final_validate as fv

from . import const, generate, schema
from .schema import TSchema


def create_entities_schema(
    entities: dict[str, TSchema],
    get_entity_validation_schema: Callable[[TSchema], cv.Schema],
) -> Schema:
    entity_schema = {}
    for key, entity in entities.items():
        schema_key = (
            cv.Optional(key, entity.default_value)
            if hasattr(entity, "default_value")
            else cv.Optional(key)
        )
        entity_schema[schema_key] = get_entity_validation_schema(entity)
    return cv.Schema(entity_schema)


def create_component_schema(
    entities: dict[str, schema.EntitySchema],
    get_entity_validation_schema: Callable[[TSchema], cv.Schema],
) -> Schema:
    return (
        cv.Schema(
            {cv.GenerateID(const.CONF_OPENTHERM_ID): cv.use_id(generate.OpenthermHub)}
        )
        .extend(create_entities_schema(entities, get_entity_validation_schema))
        .extend(cv.COMPONENT_SCHEMA)
    )


def schema_validator(
    entities: dict[str, schema.EntitySchema],
    hub: bool = False,
) -> Callable[[dict[str, Any]], dict[str, Any]]:
    def _final_validate(config: dict[str, Any]) -> dict[str, Any]:
        if hub:
            hub_conf = config
        else:
            full_config = fv.full_config.get()
            hub_path = full_config.get_path_for_id(config[const.CONF_OPENTHERM_ID])[:-1]
            hub_conf = full_config.get_config_for_path(hub_path)

        device_mode = schema.DeviceMode[hub_conf.get(const.CONF_DEVICE_MODE)]

        for key, conf in config.items():
            try:
                entity = entities[key]
            except KeyError:
                continue

            if hub and entity.default_value == conf:
                continue

            if device_mode not in entity.device_mode:
                raise cv.Invalid(f"{key} is not supported in {device_mode.name} mode")

        return config

    return _final_validate
