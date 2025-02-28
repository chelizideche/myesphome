from esphome.components.zephyr.const import KEY_BOOTLOADER

from .const import BOOTLOADER_ADAFRUIT, BOOTLOADER_ADAFRUIT_NRF52_SD140_V6


class Section:
    def __init__(self, name, address, size, region):
        self.name = name
        self.address = address
        self.size = size
        self.region = region
        self.end_address = self.address + self.size

    def __str__(self):
        return (
            f"{self.name}:\n"
            f"  address: 0x{self.address:X}\n"
            f"  end_address: 0x{self.end_address:X}\n"
            f"  region: {self.region}\n"
            f"  size: 0x{self.size:X}"
        )


BOARDS_ZEPHYR = {
    "adafruit_itsybitsy_nrf52840": {
        KEY_BOOTLOADER: [BOOTLOADER_ADAFRUIT, BOOTLOADER_ADAFRUIT_NRF52_SD140_V6]
    },
}

# https://github.com/ffenix113/zigbee_home/blob/17bb7b9e9d375e756da9e38913f53303937fb66a/types/board/known_boards.go
BOOTLOADER_CONFIG = {
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: [
        Section("empty_app_offset", 0x0, 0x26000, "flash_primary"),
        Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary"),
    ]
}
