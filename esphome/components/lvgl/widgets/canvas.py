from esphome import automation, config_validation as cv
from esphome.components.display_menu_base import CONF_LABEL
from esphome.components.lvgl.automation import action_to_code
from esphome.components.lvgl.defines import CONF_OPA
from esphome.components.lvgl.lv_validation import lv_color, opacity, size
from esphome.components.lvgl.lvcode import lv
from esphome.components.lvgl.types import LvType, ObjUpdateAction
from esphome.components.lvgl.widgets.img import CONF_IMAGE
from esphome.const import CONF_COLOR, CONF_HEIGHT, CONF_ID, CONF_WIDTH

from ..defines import CONF_MAIN, literal
from ..types import WidgetType
from . import Widget, get_widgets

CONF_CANVAS = "canvas"
CONF_BUFFER_ID = "buffer_id"

lv_canvas_t = LvType("lv_canvas_t")


class CanvasType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_CANVAS,
            lv_canvas_t,
            (CONF_MAIN,),
            cv.Schema(
                {
                    cv.Required(CONF_WIDTH): size,
                    cv.Required(CONF_HEIGHT): size,
                }
            ),
        )

    def get_uses(self):
        return "img", CONF_IMAGE, CONF_LABEL

    async def to_code(self, w: Widget, config):
        width = config[CONF_WIDTH]
        height = config[CONF_HEIGHT]
        lv.canvas_set_buffer(
            w.obj,
            lv.custom_mem_alloc(
                literal(f"LV_CANVAS_BUF_SIZE_TRUE_COLOR({width}, {height})")
            ),
            width,
            height,
            literal("LV_IMG_CF_TRUE_COLOR"),
        )


@automation.register_action(
    "lvgl.canvas.fill",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_canvas_t),
            cv.Required(CONF_COLOR): lv_color,
            cv.Optional(CONF_OPA, default="COVER"): opacity,
        },
    ),
)
async def canvas_fill(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    color = await lv_color.process(config[CONF_COLOR])
    opa = await opacity.process(config[CONF_OPA])

    async def do_fill(w: Widget):
        lv.canvas_fill_bg(w.obj, color, opa)

    return await action_to_code(widget, do_fill, action_id, template_arg, args)


canvas_spec = CanvasType()
