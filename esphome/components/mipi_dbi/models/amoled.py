from esphome.components.mipi_dbi import CONF_DRAW_ROUNDING
from esphome.const import CONF_BRIGHTNESS, CONF_SWAP_XY

from . import (
    BRIGHTNESS,
    MIPI,
    NORON,
    PAGESEL,
    PIXFMT,
    SWIRE1,
    SWIRE2,
    TEON,
    WRAM,
    DriverChip,
    cmd,
    delay,
)

RM67162 = DriverChip(
    "RM67162",
    {CONF_BRIGHTNESS: 0xD0},
    cmd(PIXFMT, 0x55),
    cmd(BRIGHTNESS, 0),
)

RM690B0 = DriverChip(
    "RM690B0",
    {CONF_BRIGHTNESS: 0xD0},
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
)

AXS15231 = DriverChip(
    "AXS15231",
    {CONF_DRAW_ROUNDING: 8, CONF_SWAP_XY: False, CONF_BRIGHTNESS: 0xD0},
    cmd(0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5),
    cmd(0xC1, 0x33),
    cmd(0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
)

chips = (AXS15231, RM67162, RM690B0)
