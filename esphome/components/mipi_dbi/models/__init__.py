SW_RESET_CMD = 0x01
SLEEP_IN = 0x10
SLEEP_OUT = 0x11
NORON = 0x13
INVERT_OFF = 0x20
INVERT_ON = 0x21
ALL_ON = 0x23
WRAM = 0x24
MIPI = 0x26
DISPLAY_OFF = 0x28
DISPLAY_ON = 0x29
RASET = 0x2B
CASET = 0x2A
WDATA = 0x2C
TEON = 0x35
MADCTL_CMD = 0x36
PIXFMT = 0x3A
BRIGHTNESS = 0x51
SWIRE1 = 0x5A
SWIRE2 = 0x5B
PAGESEL = 0xFE


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
    chips = {}

    def __init__(self, name: str, defaults=None, *initsequence):
        name = name.upper()
        self.name = name
        assert all(isinstance(x, tuple) for x in initsequence)
        self.initsequence = sum(initsequence, ())
        self.defaults = defaults or {}
