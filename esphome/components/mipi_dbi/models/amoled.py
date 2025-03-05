from esphome.components.mipi_dbi import MODE_RGB
from esphome.components.spi import TYPE_QUAD
from esphome.const import CONF_BRIGHTNESS, CONF_COLOR_ORDER

from . import DriverChip, delay
from .commands import (
    BRIGHTNESS,
    MIPI,
    NORON,
    PAGESEL,
    PIXFMT,
    SWIRE1,
    SWIRE2,
    TEON,
    WRAM,
)

RM67162 = DriverChip(
    name="RM67162",
    defaults={CONF_BRIGHTNESS: 0xD0, CONF_COLOR_ORDER: MODE_RGB},
    modes=(TYPE_QUAD,),
    initsequence=(
        (PIXFMT, 0x55),
        (BRIGHTNESS, 0),
    ),
)

RM690B0 = DriverChip(
    "RM690B0",
    defaults={CONF_BRIGHTNESS: 0xD0, CONF_COLOR_ORDER: MODE_RGB},
    modes=(TYPE_QUAD,),
    initsequence=(
        (PAGESEL, 0x20),
        (MIPI, 0x0A),
        (WRAM, 0x80),
        (SWIRE1, 0x51),
        (SWIRE2, 0x2E),
        (PAGESEL, 0x00),
        (0xC2, 0x00),
        delay(10),
        (TEON, 0x00),
        (PIXFMT, 0x55),
        (NORON,),
    ),
)

chips = (RM67162, RM690B0)
