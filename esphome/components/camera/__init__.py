from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_TRIGGER_ID

CODEOWNERS = ["@DT-art1"]

CONF_ON_STREAM_START = "on_stream_start"
CONF_ON_STREAM_STOP = "on_stream_stop"
CONF_ON_IMAGE = "on_image"

camera_ns = cg.esphome_ns.namespace("camera")
Camera = camera_ns.class_("Camera", cg.Component, cg.EntityBase)
CameraImageData = camera_ns.struct("CameraImageData")
CameraImageTrigger = camera_ns.class_(
    "CameraImageTrigger", automation.Trigger.template()
)
CameraStreamStartTrigger = camera_ns.class_(
    "CameraStreamStartTrigger",
    automation.Trigger.template(),
)
CameraStreamStopTrigger = camera_ns.class_(
    "CameraStreamStopTrigger",
    automation.Trigger.template(),
)

CAMERA_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_STREAM_START): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraStreamStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_STREAM_STOP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraStreamStopTrigger),
            }
        ),
        cv.Optional(CONF_ON_IMAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraImageTrigger),
            }
        ),
    }
)


async def to_code(config):
    cg.add_define("USE_CAMERA")


async def setup_camera(var, config):
    for conf in config.get(CONF_ON_STREAM_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STREAM_STOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(CameraImageData, "image")], conf)
