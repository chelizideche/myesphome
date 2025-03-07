from esphome.components.mipi_spi import MODE_RGB
from esphome.components.spi import TYPE_QUAD

from . import DriverChip, delay
from .commands import MIPI, NORON, PAGESEL, SLPOUT, SWIRE1, SWIRE2, TEON, WRAM

RM67162 = DriverChip(
    name="RM67162",
    brightness=0xD0,
    color_order=MODE_RGB,
    width=480,
    modes=(TYPE_QUAD,),
    initsequence=(SLPOUT,),  # Requires early SLPOUT
)

T_DISPLAY_S3_AMOLED = RM67162.derive("T-DISPLAY-S3-AMOLED", width=240, height=536)

RM690B0 = DriverChip(
    "RM690B0",
    brightness=0xD0,
    color_order=MODE_RGB,
    width=480,
    height=600,
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
        (NORON,),
    ),
)

T4_S3_AMOLED = RM690B0.derive("T4-S3", width=450, offset_width=16)

models = {}
