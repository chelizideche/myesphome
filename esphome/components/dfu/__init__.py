import esphome.codegen as cg
from esphome.components import output
from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_data
from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import CONF_ID, PLATFORM_NRF52

CODEOWNERS = ["@tomaszduda23"]

dfu_ns = cg.esphome_ns.namespace("dfu")
DeviceFirmwareUpdate = dfu_ns.class_("DeviceFirmwareUpdate", cg.Component)

CONF_RESET_OUTPUT = "reset_output"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DeviceFirmwareUpdate),
            cv.Required(CONF_RESET_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(PLATFORM_NRF52),
)


def _validate_mcumgr(config):
    bootloader = zephyr_data()[KEY_BOOTLOADER]
    if bootloader == BOOTLOADER_MCUBOOT:
        raise cv.Invalid(f"'{bootloader}' bootloader does not support DFU")


def _final_validate(config):
    _validate_mcumgr(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    reset_output = await cg.get_variable(config[CONF_RESET_OUTPUT])
    cg.add(var.set_reset_output(reset_output))
    zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
    await cg.register_component(var, config)
