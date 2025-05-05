from dataclasses import dataclass
from typing import Any

import esphome.codegen as cg
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_METHOD,
    CONF_PIN,
    CONF_SPEED,
)
from esphome.core import CORE

from .const import (
    CHIP_400KBPS,
    CHIP_800KBPS,
    CHIP_APA106,
    CHIP_DOTSTAR,
    CHIP_HD108,
    CHIP_LC8812,
    CHIP_LPD6803,
    CHIP_LPD8806,
    CHIP_P9813,
    CHIP_SK6812,
    CHIP_SK9822,
    CHIP_SM16716,
    CHIP_TLC59711,
    CHIP_TM1814,
    CHIP_TM1829,
    CHIP_TM1914,
    CHIP_WS2801,
    CHIP_WS2811,
    CHIP_WS2812,
    CHIP_WS2812X,
    CHIP_WS2813,
    CONF_ASYNC,
    CONF_BUS,
    ONE_WIRE_CHIPS,
    TWO_WIRE_CHIPS,
    RMT_COMPATIBLE_CHIPS,
    I2S_COMPATIBLE_CHIPS,
    BIT_BANG_COMPATIBLE_CHIPS,
)

METHOD_BIT_BANG = "bit_bang"
METHOD_ESP8266_UART = "esp8266_uart"
METHOD_ESP8266_DMA = "esp8266_dma"
METHOD_ESP32_RMT = "esp32_rmt"
METHOD_ESP32_I2S = "esp32_i2s"
METHOD_SPI = "spi"

CHANNEL_DYNAMIC = "dynamic"
BUS_DYNAMIC = "dynamic"
SPI_BUS_VSPI = "vspi"
SPI_BUS_HSPI = "hspi"
SPI_SPEEDS = [40e6, 20e6, 10e6, 5e6, 2e6, 1e6, 500e3]


def _esp32_rmt_default_channel():
    return {
        VARIANT_ESP32C3: 1,
        VARIANT_ESP32S2: 1,
        VARIANT_ESP32S3: 1,
    }.get(get_esp32_variant(), 6)


def _validate_esp32_rmt_channel(value):
    if isinstance(value, str) and value.lower() == CHANNEL_DYNAMIC:
        value = CHANNEL_DYNAMIC
    else:
        value = cv.int_(value)
    variant_channels = {
        VARIANT_ESP32: [0, 1, 2, 3, 4, 5, 6, 7, CHANNEL_DYNAMIC],
        VARIANT_ESP32C3: [0, 1, CHANNEL_DYNAMIC],
        VARIANT_ESP32S2: [0, 1, 2, 3, CHANNEL_DYNAMIC],
        VARIANT_ESP32S3: [0, 1, 2, 3, CHANNEL_DYNAMIC],
    }
    variant = get_esp32_variant()
    if variant not in variant_channels:
        raise cv.Invalid(f"{variant} does not support the rmt method")
    if value not in variant_channels[variant]:
        raise cv.Invalid(f"{variant} does not support rmt channel {value}")
    return value


def _esp32_i2s_default_bus():
    return {
        VARIANT_ESP32: 1,
        VARIANT_ESP32S2: 0,
    }.get(get_esp32_variant(), 0)


def _validate_esp32_i2s_bus(value):
    if isinstance(value, str) and value.lower() == BUS_DYNAMIC:
        value = BUS_DYNAMIC
    else:
        value = cv.int_(value)
    variant_buses = {
        VARIANT_ESP32: [0, 1, BUS_DYNAMIC],
        VARIANT_ESP32S2: [0, BUS_DYNAMIC],
    }
    variant = get_esp32_variant()
    if variant not in variant_buses:
        raise cv.Invalid(f"{variant} does not support the i2s method")
    if value not in variant_buses[variant]:
        raise cv.Invalid(f"{variant} does not support i2s bus {value}")
    return value


neo_ns = cg.global_ns
# Base shared classes
NeoEspNotInverted = neo_ns.class_("NeoEspNotInverted")

# I2S Method classes
NeoEsp32I2sMethodBase = neo_ns.class_("NeoEsp32I2sMethodBase")
NeoEsp32I2sBusZero = neo_ns.class_("NeoEsp32I2sBusZero")
NeoEsp32I2sBusOne = neo_ns.class_("NeoEsp32I2sBusOne")
NeoEsp32I2sBusN = neo_ns.class_("NeoEsp32I2sBusN")
NeoEsp32I2sNotInverted = neo_ns.class_("NeoEsp32I2sNotInverted", NeoEspNotInverted)
NeoEsp32I2sInverted = neo_ns.class_("NeoEsp32I2sInverted")
NeoEsp32I2sCadence = neo_ns.class_("NeoEsp32I2sCadence")
NeoEsp32I2sSpeedWs2812x = neo_ns.class_("NeoEsp32I2sSpeedWs2812x")

# RMT Method classes
NeoEsp32RmtMethodBase = neo_ns.class_("NeoEsp32RmtMethodBase")
NeoEsp32RmtSpeedBase = neo_ns.class_("NeoEsp32RmtSpeedBase")
NeoEsp32RmtSpeed = neo_ns.class_("NeoEsp32RmtSpeed")


def _bit_bang_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEspBitBangMethod.h
    # Some chips are only aliases
    chip = {
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2812: CHIP_800KBPS,
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
    }.get(chip, chip)

    if chip not in BIT_BANG_COMPATIBLE_CHIPS:
        raise cv.Invalid(f"Bit Bang method is not supported for chip {chip}.")

    # Restored lookup table with 'pinset_inverted' support
    lookup = {
        CHIP_WS2811: (neo_ns.NeoEspBitBangSpeedWs2811, False),
        CHIP_WS2812X: (neo_ns.NeoEspBitBangSpeedWs2812x, False),
        CHIP_SK6812: (neo_ns.NeoEspBitBangSpeedSk6812, False),
        CHIP_TM1814: (neo_ns.NeoEspBitBangSpeedTm1814, True),
        CHIP_TM1829: (neo_ns.NeoEspBitBangSpeedTm1829, True),
        CHIP_800KBPS: (neo_ns.NeoEspBitBangSpeed800Kbps, False),
        CHIP_400KBPS: (neo_ns.NeoEspBitBangSpeed400Kbps, False),
        CHIP_APA106: (neo_ns.NeoEspBitBangSpeedApa106, False),
    }

    # Direct lookup of speed and pinset_inverted
    speed, pinset_inverted = lookup[chip]

    # Compute the pinset based on 'inverted' logic
    pinset = {
        False: neo_ns.NeoEspPinset,
        True: neo_ns.NeoEspPinsetInverted,
    }[inverted != pinset_inverted]

    return neo_ns.NeoEspBitBangMethodBase.template(speed, pinset)


def _bit_bang_extra_validate(config):
    pin = config[CONF_PIN]
    if CORE.is_esp8266 and not (0 <= pin <= 15):
        # Due to use of w1ts
        raise cv.Invalid("Bit bang only supports pins GPIO0-GPIO15 on ESP8266")
    if CORE.is_esp32 and not (0 <= pin <= 31):
        raise cv.Invalid("Bit bang only supports pins GPIO0-GPIO31 on ESP32")


def _esp8266_uart_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp8266UartMethod.h
    uart_context, uart_base = {
        False: (neo_ns.NeoEsp8266UartContext, neo_ns.NeoEsp8266Uart),
        True: (neo_ns.NeoEsp8266UartInterruptContext, neo_ns.NeoEsp8266AsyncUart),
    }[config[CONF_ASYNC]]
    uart_feature = {
        0: neo_ns.UartFeature0,
        1: neo_ns.UartFeature1,
    }[config[CONF_BUS]]
    # Some chips are only aliases
    chip = {
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        CHIP_WS2812X: (neo_ns.NeoEsp8266UartSpeedWs2812x, False),
        CHIP_SK6812: (neo_ns.NeoEsp8266UartSpeedSk6812, False),
        CHIP_TM1814: (neo_ns.NeoEsp8266UartSpeedTm1814, True),
        CHIP_TM1829: (neo_ns.NeoEsp8266UartSpeedTm1829, True),
        CHIP_800KBPS: (neo_ns.NeoEsp8266UartSpeed800Kbps, False),
        CHIP_400KBPS: (neo_ns.NeoEsp8266UartSpeed400Kbps, False),
        CHIP_APA106: (neo_ns.NeoEsp8266UartSpeedApa106, False),
    }
    speed, uart_inverted = lookup[chip]
    # For tm variants opposite of inverted is needed
    inv = {
        False: neo_ns.NeoEsp8266UartNotInverted,
        True: neo_ns.NeoEsp8266UartInverted,
    }[inverted != uart_inverted]
    return neo_ns.NeoEsp8266UartMethodBase.template(
        speed, uart_base.template(uart_feature, uart_context), inv
    )


def _esp8266_uart_extra_validate(config):
    pin = config[CONF_PIN]
    bus = config[CONF_METHOD][CONF_BUS]
    right_pin = {
        0: 1,  # U0TXD
        1: 2,  # U1TXD
    }[bus]
    if pin != right_pin:
        raise cv.Invalid(f"ESP8266 uart bus {bus} only supports pin GPIO{right_pin}")


def _esp8266_dma_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp8266DmaMethod.h
    # Some chips are only aliases
    chip = {
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        (CHIP_WS2812X, False): neo_ns.NeoEsp8266DmaSpeedWs2812x,
        (CHIP_SK6812, False): neo_ns.NeoEsp8266DmaSpeedSk6812,
        (CHIP_TM1814, True): neo_ns.NeoEsp8266DmaInvertedSpeedTm1814,
        (CHIP_TM1829, True): neo_ns.NeoEsp8266DmaInvertedSpeedTm1829,
        (CHIP_800KBPS, False): neo_ns.NeoEsp8266DmaSpeed800Kbps,
        (CHIP_400KBPS, False): neo_ns.NeoEsp8266DmaSpeed400Kbps,
        (CHIP_APA106, False): neo_ns.NeoEsp8266DmaSpeedApa106,
        (CHIP_WS2812X, True): neo_ns.NeoEsp8266DmaInvertedSpeedWs2812x,
        (CHIP_SK6812, True): neo_ns.NeoEsp8266DmaInvertedSpeedSk6812,
        (CHIP_TM1814, False): neo_ns.NeoEsp8266DmaSpeedTm1814,
        (CHIP_TM1829, False): neo_ns.NeoEsp8266DmaSpeedTm1829,
        (CHIP_800KBPS, True): neo_ns.NeoEsp8266DmaInvertedSpeed800Kbps,
        (CHIP_400KBPS, True): neo_ns.NeoEsp8266DmaInvertedSpeed400Kbps,
        (CHIP_APA106, True): neo_ns.NeoEsp8266DmaInvertedSpeedApa106,
    }
    speed = lookup[(chip, inverted)]
    return neo_ns.NeoEsp8266DmaMethodBase.template(speed)


def _esp8266_dma_extra_validate(config):
    if config[CONF_PIN] != 3:
        raise cv.Invalid("ESP8266 dma method only supports pin GPIO3")


def _esp32_rmt_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp32RmtMethod.h
    # Resolve chip aliases
    chip_aliases_rmt = {
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
    }
    chip = chip_aliases_rmt.get(chip, chip)

    # Ensure the chip is supported for the RMT method
    if chip not in RMT_COMPATIBLE_CHIPS:
        raise cv.Invalid(f"RMT method is not supported for chip {chip}.")

    # Map channel configuration
    channel = {
        0: neo_ns.NeoEsp32RmtChannel0,
        1: neo_ns.NeoEsp32RmtChannel1,
        2: neo_ns.NeoEsp32RmtChannel2,
        3: neo_ns.NeoEsp32RmtChannel3,
        4: neo_ns.NeoEsp32RmtChannel4,
        5: neo_ns.NeoEsp32RmtChannel5,
        6: neo_ns.NeoEsp32RmtChannel6,
        7: neo_ns.NeoEsp32RmtChannel7,
        CHANNEL_DYNAMIC: neo_ns.NeoEsp32RmtChannelN,
    }[config[CONF_CHANNEL]]

    # Define RMT speed settings, including inverted variants
    lookup = {
        (CHIP_400KBPS, False): neo_ns.NeoEsp32RmtSpeed400Kbps,
        (CHIP_400KBPS, True): neo_ns.NeoEsp32RmtInvertedSpeed400Kbps,
        (CHIP_800KBPS, False): neo_ns.NeoEsp32RmtSpeed800Kbps,
        (CHIP_800KBPS, True): neo_ns.NeoEsp32RmtInvertedSpeed800Kbps,
        (CHIP_APA106, False): neo_ns.NeoEsp32RmtSpeedApa106,
        (CHIP_APA106, True): neo_ns.NeoEsp32RmtInvertedSpeedApa106,
        (CHIP_SK6812, False): neo_ns.NeoEsp32RmtSpeedSk6812,
        (CHIP_SK6812, True): neo_ns.NeoEsp32RmtInvertedSpeedSk6812,
        (CHIP_TM1814, False): neo_ns.NeoEsp32RmtSpeedTm1814,
        (CHIP_TM1814, True): neo_ns.NeoEsp32RmtInvertedSpeedTm1814,
        (CHIP_TM1829, False): neo_ns.NeoEsp32RmtSpeedTm1829,
        (CHIP_TM1829, True): neo_ns.NeoEsp32RmtInvertedSpeedTm1829,
        (CHIP_TM1914, False): neo_ns.NeoEsp32RmtSpeedTm1914,
        (CHIP_TM1914, True): neo_ns.NeoEsp32RmtInvertedSpeedTm1914,
        (CHIP_WS2811, False): neo_ns.NeoEsp32RmtSpeedWs2811,
        (CHIP_WS2811, True): neo_ns.NeoEsp32RmtInvertedSpeedWs2811,
        (CHIP_WS2812X, False): neo_ns.NeoEsp32RmtSpeedWs2812x,
        (CHIP_WS2812X, True): neo_ns.NeoEsp32RmtInvertedSpeedWs2812x,
    }

    # Ensure chip with inversion setting is in lookup
    if (chip, inverted) not in lookup:
        raise cv.Invalid(
            f"Chip {chip} with inverted={inverted} does not have a defined RMT speed configuration."
        )

    # Retrieve speed configuration
    speed = lookup[(chip, inverted)]

    # Return the configured RMT method
    return neo_ns.NeoEsp32RmtMethodBase.template(speed, channel)


def _esp32_i2s_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp32I2sMethod.h
    # Normalize chip with aliases
    chip_aliases_i2s = {
        CHIP_LC8812: CHIP_SK6812,
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2812: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
    }
    chip = chip_aliases_i2s.get(chip, chip)

    # Ensure chip is compatible with I2S
    if chip not in I2S_COMPATIBLE_CHIPS:
        raise cv.Invalid(
            f"I2S method is not supported for chip {chip}. Please check the compatibility list."
        )

    # Map bus number to method class
    bus_number = config[CONF_BUS]
    method_types = {
        # Bus 0
        (0, CHIP_SK6812, False): neo_ns.NeoEsp32I2s0Sk6812Method,
        (0, CHIP_TM1814, False): neo_ns.NeoEsp32I2s0Tm1814Method,
        (0, CHIP_TM1914, False): neo_ns.NeoEsp32I2s0Tm1914Method,
        (0, CHIP_WS2812X, False): neo_ns.NeoEsp32I2s0Ws2812xMethod,
        # Bus 0 Inverted
        (0, CHIP_SK6812, True): neo_ns.NeoEsp32I2s0Sk6812InvertedMethod,
        (0, CHIP_TM1814, True): neo_ns.NeoEsp32I2s0Tm1814InvertedMethod,
        (0, CHIP_TM1914, True): neo_ns.NeoEsp32I2s0Tm1914InvertedMethod,
        (0, CHIP_WS2812X, True): neo_ns.NeoEsp32I2s0Ws2812xInvertedMethod,
        # Bus 1
        (1, CHIP_SK6812, False): neo_ns.NeoEsp32I2s1Sk6812Method,
        (1, CHIP_TM1814, False): neo_ns.NeoEsp32I2s1Tm1814Method,
        (1, CHIP_TM1914, False): neo_ns.NeoEsp32I2s1Tm1914Method,
        (1, CHIP_WS2812X, False): neo_ns.NeoEsp32I2s1Ws2812xMethod,
        # Bus 1 Inverted
        (1, CHIP_SK6812, True): neo_ns.NeoEsp32I2s1Sk6812InvertedMethod,
        (1, CHIP_TM1814, True): neo_ns.NeoEsp32I2s1Tm1814InvertedMethod,
        (1, CHIP_TM1914, True): neo_ns.NeoEsp32I2s1Tm1914InvertedMethod,
        (1, CHIP_WS2812X, True): neo_ns.NeoEsp32I2s1Ws2812xInvertedMethod,
    }

    # If using dynamic bus, use the corresponding method
    if bus_number == BUS_DYNAMIC:
        method_types = {
            (BUS_DYNAMIC, CHIP_SK6812, False): neo_ns.NeoEsp32I2sNSk6812Method,
            (BUS_DYNAMIC, CHIP_SK6812, True): neo_ns.NeoEsp32I2sNSk6812InvertedMethod,
            (BUS_DYNAMIC, CHIP_TM1814, False): neo_ns.NeoEsp32I2sNTm1814Method,
            (BUS_DYNAMIC, CHIP_TM1814, True): neo_ns.NeoEsp32I2sNTm1814InvertedMethod,
            (BUS_DYNAMIC, CHIP_TM1914, False): neo_ns.NeoEsp32I2sNTm1914Method,
            (BUS_DYNAMIC, CHIP_TM1914, True): neo_ns.NeoEsp32I2sNTm1914InvertedMethod,
            (BUS_DYNAMIC, CHIP_WS2812X, False): neo_ns.NeoEsp32I2sNWs2812xMethod,
            (BUS_DYNAMIC, CHIP_WS2812X, True): neo_ns.NeoEsp32I2sNWs2812xInvertedMethod,
        }

    method_key = (bus_number, chip, inverted)
    if method_key not in method_types:
        raise cv.Invalid(
            f"No predefined I2S method available for bus {bus_number}, chip {chip}, inverted={inverted}"
        )

    # Return the method type directly without .template()
    return method_types[method_key]


def _spi_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/TwoWireSpiImple.h
    spi_imple = {
        None: neo_ns.TwoWireSpiImple,
        SPI_BUS_VSPI: neo_ns.TwoWireSpiImple,
        SPI_BUS_HSPI: neo_ns.TwoWireHspiImple,
    }[config.get(CONF_BUS)]

    # Map SPI speeds
    spi_speed = {
        40e6: neo_ns.SpiSpeed40Mhz,
        20e6: neo_ns.SpiSpeed20Mhz,
        10e6: neo_ns.SpiSpeed10Mhz,
        5e6: neo_ns.SpiSpeed5Mhz,
        2e6: neo_ns.SpiSpeed2Mhz,
        1e6: neo_ns.SpiSpeed1Mhz,
        500e3: neo_ns.SpiSpeed500Khz,
    }[config[CONF_SPEED]]

    # Chip-specific base methods
    chip_method_base = {
        CHIP_DOTSTAR: neo_ns.DotStarMethodBase,
        CHIP_HD108: neo_ns.Hd108MethodBase,
        CHIP_LPD6803: neo_ns.Lpd6803MethodBase,
        CHIP_LPD8806: neo_ns.Lpd8806MethodBase,
        CHIP_P9813: neo_ns.P9813MethodBase,
        CHIP_SK9822: neo_ns.Sk9822MethodBase,
        CHIP_SM16716: neo_ns.Sm16716MethodBase,
        CHIP_TLC59711: neo_ns.Tlc59711MethodBase,
        CHIP_WS2801: neo_ns.Ws2801MethodBase,
    }

    # Ensure chip compatibility
    if chip not in chip_method_base:
        raise cv.Invalid(
            f"Chip {chip} does not have a defined SPI configuration. Please check the compatibility list."
        )

    # Return the specific method base with its SPI implementation and speed
    return chip_method_base[chip].template(spi_imple.template(spi_speed))


def _spi_extra_validate(config):
    if CORE.is_esp32:
        return

    if config[CONF_DATA_PIN] != 13 and config[CONF_CLOCK_PIN] != 14:
        raise cv.Invalid(
            "SPI only supports pins GPIO13 for data and GPIO14 for clock on ESP8266"
        )


@dataclass
class MethodDescriptor:
    method_schema: Any
    to_code: Any
    supported_chips: list[str]
    extra_validate: Any = None


METHODS = {
    METHOD_BIT_BANG: MethodDescriptor(
        method_schema={},
        to_code=_bit_bang_to_code,
        extra_validate=_bit_bang_extra_validate,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP8266_UART: MethodDescriptor(
        method_schema=cv.All(
            cv.only_on_esp8266,
            {
                cv.Optional(CONF_ASYNC, default=False): cv.boolean,
                cv.Optional(CONF_BUS, default=1): cv.int_range(min=0, max=1),
            },
        ),
        extra_validate=_esp8266_uart_extra_validate,
        to_code=_esp8266_uart_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP8266_DMA: MethodDescriptor(
        method_schema=cv.All(cv.only_on_esp8266, {}),
        extra_validate=_esp8266_dma_extra_validate,
        to_code=_esp8266_dma_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP32_RMT: MethodDescriptor(
        method_schema=cv.All(
            cv.only_on_esp32,
            {
                cv.Optional(
                    CONF_CHANNEL, default=_esp32_rmt_default_channel
                ): _validate_esp32_rmt_channel,
            },
        ),
        to_code=_esp32_rmt_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP32_I2S: MethodDescriptor(
        method_schema=cv.All(
            cv.only_on_esp32,
            {
                cv.Optional(
                    CONF_BUS, default=_esp32_i2s_default_bus
                ): _validate_esp32_i2s_bus,
            },
        ),
        to_code=_esp32_i2s_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_SPI: MethodDescriptor(
        method_schema={
            cv.Optional(CONF_BUS): cv.All(
                cv.only_on_esp32, cv.one_of(SPI_BUS_VSPI, SPI_BUS_HSPI, lower=True)
            ),
            cv.Optional(CONF_SPEED, default="10MHz"): cv.All(
                cv.frequency, cv.one_of(*SPI_SPEEDS)
            ),
        },
        to_code=_spi_to_code,
        extra_validate=_spi_extra_validate,
        supported_chips=TWO_WIRE_CHIPS,
    ),
}
