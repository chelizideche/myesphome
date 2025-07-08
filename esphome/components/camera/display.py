import esphome.codegen as cg
from esphome.components import camera, display
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA

CODEOWNERS = ["@DT-art1"]

CameraOverlayDisplay = camera.camera_ns.class_("CameraOverlayDisplay", display.Display)
CameraOverlayDisplayRef = CameraOverlayDisplay.operator("ref")

CONFIG_SCHEMA = cv.All(
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(CameraOverlayDisplay),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    if lambda_config := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config,
            [
                (CameraOverlayDisplayRef, "it"),
                (camera.CameraIncrementalContextRef, "context"),
            ],
            return_type=cg.void,
        )
        cg.add(var.set_my_writer(lambda_))
