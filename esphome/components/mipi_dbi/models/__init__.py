from esphome.components.spi import TYPE_QUAD, TYPE_SINGLE
import esphome.config_validation as cv

MADCTL_MY = 0x80  # Bit 7 Bottom to top
MADCTL_MX = 0x40  # Bit 6 Right to left
MADCTL_MV = 0x20  # Bit 5 Reverse Mode
MADCTL_ML = 0x10  # Bit 4 LCD refresh Bottom to top
MADCTL_RGB = 0x00  # Bit 3 Red-Green-Blue pixel order
MADCTL_BGR = 0x08  # Bit 3 Blue-Green-Red pixel order
MADCTL_MH = 0x04  # Bit 2 LCD refresh right to left


def cmd(c, *args):
    """
    Add a command sequence to the init sequence
    :param c: The command (8 bit)
    :param args: zero or more arguments (8 bit values)
    """
    return (c, len(args)) + tuple(args)


def delay(ms):
    return ms, 0xFF


class DriverChip:
    def __init__(
        self, name: str, defaults=None, modes=(TYPE_SINGLE, TYPE_QUAD), initsequence=()
    ):
        name = name.upper()
        self.name = name
        self.modes = modes
        assert all(isinstance(x, tuple) for x in initsequence)
        self.initsequence = sum(initsequence, ())
        self.defaults = defaults or {}

    def get_default(self, key, fallback=False):
        return self.defaults.get(key, fallback)

    def option(self, name, fallback=False):
        return cv.Optional(name, default=self.get_default(name, fallback))
