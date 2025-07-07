from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_ID, CONF_TRIGGER_ID, CONF_WIDTH
from esphome.core.entity_helpers import setup_entity

CODEOWNERS = ["@DT-art1", "@bdraco"]

CONF_IDLE_UPDATE_INTERVAL = "idle_update_interval"
CONF_MAX_UPDATE_INTERVAL = "max_update_interval"

CONF_ENCODER_ID = "encoder_id"
CONF_ENCODER_BUFFER_SIZE = "encoder_buffer_size"
CONF_ENCODER_BUFFER_GROW = "encoder_buffer_grow"
CONF_ENCODER_MCU_COUNT = "encoder_mcu_count"

CONF_ON_STREAM_START = "on_stream_start"
CONF_ON_STREAM_STOP = "on_stream_stop"
CONF_ON_IMAGE = "on_image"
CONF_ON_CAPTURE_IMAGE = "on_capture_image"
CONF_ON_OVERLAY = "on_overlay"

camera_ns = cg.esphome_ns.namespace("camera")
Camera = camera_ns.class_("CameraImpl", cg.Component, cg.EntityBase)
Encoder = camera_ns.class_("JPEGEncoderImpl")

CameraImageData = camera_ns.struct("CameraImageData")
CameraImageSpec = camera_ns.struct("CameraImageSpec")
CameraIncrementalContext = camera_ns.struct("CameraIncrementalContext")
CameraIncrementalContextRef = CameraIncrementalContext.operator("ref")

CameraImageImpl = camera_ns.class_("CameraImageImpl")
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
CameraCaptureImageTrigger = camera_ns.class_(
    "CameraCaptureImageTrigger", automation.Trigger.template()
)
CameraOverlayTrigger = camera_ns.class_(
    "CameraOverlayTrigger", automation.Trigger.template()
)

ImageFormat = camera_ns.enum("ImageFormat")

EncoderQuality = camera_ns.enum("EncoderQuality")
EncoderSubsampling = camera_ns.enum("EncoderSubsampling")

CONF_IMAGE_FORMAT = "image_format"
CONF_ENCODER_QUALITY = "encoder_quality"
CONF_ENCODER_SUBSAMPLING = "encoder_subsampling"

CONF_IMAGE_FORMAT_SELECTS = {
    "GRAYSCALE": ImageFormat.IMAGE_FORMAT_GRAYSCALE,
    "RGB565": ImageFormat.IMAGE_FORMAT_RGB565,
    "RGB888": ImageFormat.IMAGE_FORMAT_RGB888,
}
CONF_ENCODER_SUBSAMPLING_SELECTS = {
    "444": EncoderSubsampling.ENCODER_SUBSAMPLING_444,
    "420": EncoderSubsampling.ENCODER_SUBSAMPLING_420,
}
CONF_ENCODER_QUALITY_SELECTS = {
    "BEST": EncoderQuality.ENCODER_QUALITY_BEST,
    "HIGH": EncoderQuality.ENCODER_QUALITY_HIGH,
    "MED": EncoderQuality.ENCODER_QUALITY_MED,
    "LOW": EncoderQuality.ENCODER_QUALITY_LOW,
}

MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

CAMERA_AUTOMATION_SCHEMA = cv.Schema(
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
        cv.Optional(CONF_ON_CAPTURE_IMAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    CameraCaptureImageTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_OVERLAY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraOverlayTrigger),
            }
        ),
    }
)

ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ENCODER_ID): cv.declare_id(Encoder),
        cv.Optional(CONF_ENCODER_QUALITY, default="BEST"): cv.enum(
            CONF_ENCODER_QUALITY_SELECTS, upper=True
        ),
        cv.Optional(CONF_ENCODER_SUBSAMPLING, default="444"): cv.enum(
            CONF_ENCODER_SUBSAMPLING_SELECTS, upper=True
        ),
    }
)

CAMERA_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_HEIGHT): cv.int_range(0),
            cv.Required(CONF_WIDTH): cv.int_range(0),
            cv.Required(CONF_IMAGE_FORMAT): cv.enum(
                CONF_IMAGE_FORMAT_SELECTS, upper=True
            ),
            cv.Optional(CONF_IDLE_UPDATE_INTERVAL, default=0): cv.int_range(0),
            cv.Optional(CONF_MAX_UPDATE_INTERVAL, default=100): cv.int_range(0),
            cv.Optional(CONF_ENCODER_BUFFER_SIZE, default=4096): cv.int_range(1024),
            cv.Optional(CONF_ENCODER_BUFFER_GROW, default=4096): cv.int_range(0),
            cv.Optional(CONF_ENCODER_MCU_COUNT, default=256): cv.int_range(0),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CAMERA_AUTOMATION_SCHEMA)
    .extend(ENCODER_SCHEMA)
    .extend(cv.ENTITY_BASE_SCHEMA)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Camera),
    }
).extend(CAMERA_SCHEMA)


async def setup_camera(var, config):
    cg.add(var.set_idle_update_interval(config[CONF_IDLE_UPDATE_INTERVAL]))
    cg.add(var.set_max_update_interval(config[CONF_MAX_UPDATE_INTERVAL]))
    cg.add(var.set_encoder_buffer_size(config[CONF_ENCODER_BUFFER_SIZE]))
    cg.add(var.set_encoder_buffer_grow(config[CONF_ENCODER_BUFFER_GROW]))
    cg.add(
        var.set_camera_image_spec(
            config[CONF_WIDTH], config[CONF_HEIGHT], config[CONF_IMAGE_FORMAT]
        )
    )

    await setup_entity(var, config, "camera")
    await setup_camera_automation(var, config)
    await cg.register_component(var, config)
    if CONF_ENCODER_ID in config:
        await setup_encoder(var, config)


async def setup_camera_automation(var, config):
    for conf in config.get(CONF_ON_STREAM_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STREAM_STOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(CameraImageData, "image")], conf)

    for conf in config.get(CONF_ON_CAPTURE_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (CameraImageData, "image"),
                (CameraImageSpec, "spec"),
                (CameraIncrementalContextRef, "context"),
            ],
            conf,
        )

    for conf in config.get(CONF_ON_OVERLAY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (CameraImageData, "image"),
                (CameraImageSpec, "spec"),
                (CameraIncrementalContextRef, "context"),
            ],
            conf,
        )


async def setup_encoder(var, config):
    cg.add_build_flag("-DUSE_CAMERA_SW_JPEG_ENCODER")
    cg.add_library("dt-art1/jpegenc-pio", "1.0.0")

    encoder = cg.new_Pvariable(config[CONF_ENCODER_ID])
    cg.add(encoder.set_quality(config[CONF_ENCODER_QUALITY]))
    cg.add(encoder.set_subsampling(config[CONF_ENCODER_SUBSAMPLING]))
    cg.add(encoder.set_mcu_count(config[CONF_ENCODER_MCU_COUNT]))
    cg.add(var.set_encoder(encoder))


async def new_camera(config, *args):
    cg.add_define("USE_CAMERA")
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await setup_camera(var, config)
    return var


async def to_code(config):
    await new_camera(config)
