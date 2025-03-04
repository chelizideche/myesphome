from esphome.components.mipi_dbi import MODE_RGB
from esphome.components.spi import TYPE_QUAD
from esphome.const import CONF_BRIGHTNESS, CONF_COLOR_ORDER

from . import DriverChip, cmd, delay
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
        cmd(PIXFMT, 0x55),
        cmd(BRIGHTNESS, 0),
    ),
)

RM690B0 = DriverChip(
    "RM690B0",
    defaults={CONF_BRIGHTNESS: 0xD0, CONF_COLOR_ORDER: MODE_RGB},
    modes=(TYPE_QUAD,),
    initsequence=(
        cmd(PAGESEL, 0x20),
        cmd(MIPI, 0x0A),
        cmd(WRAM, 0x80),
        cmd(SWIRE1, 0x51),
        cmd(SWIRE2, 0x2E),
        cmd(PAGESEL, 0x00),
        cmd(0xC2, 0x00),
        delay(10),
        cmd(TEON, 0x00),
        cmd(PIXFMT, 0x55),
        cmd(NORON),
    ),
)

chips = (RM67162, RM690B0)
