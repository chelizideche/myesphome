"""Memory usage analyzer for ESPHome compiled binaries."""

from collections import defaultdict
import json
import logging
from pathlib import Path
import re
import subprocess

_LOGGER = logging.getLogger(__name__)

# Component namespace patterns
COMPONENT_PATTERNS = {
    "api": re.compile(r"esphome::api::"),
    "wifi": re.compile(r"esphome::wifi::"),
    "mqtt": re.compile(r"esphome::mqtt::"),
    "web_server": re.compile(r"esphome::web_server::"),
    "sensor": re.compile(r"esphome::sensor::"),
    "binary_sensor": re.compile(r"esphome::binary_sensor::"),
    "switch": re.compile(r"esphome::switch_::"),
    "light": re.compile(r"esphome::light::"),
    "cover": re.compile(r"esphome::cover::"),
    "climate": re.compile(r"esphome::climate::"),
    "fan": re.compile(r"esphome::fan::"),
    "display": re.compile(r"esphome::display::"),
    "logger": re.compile(r"esphome::logger::"),
    "ota": re.compile(r"esphome::ota::"),
    "time": re.compile(r"esphome::time::"),
    "sun": re.compile(r"esphome::sun::"),
    "text_sensor": re.compile(r"esphome::text_sensor::"),
    "script": re.compile(r"esphome::script::"),
    "interval": re.compile(r"esphome::interval::"),
    "json": re.compile(r"esphome::json::"),
    "network": re.compile(r"esphome::network::"),
    "mdns": re.compile(r"esphome::mdns::"),
    "i2c": re.compile(r"esphome::i2c::"),
    "spi": re.compile(r"esphome::spi::"),
    "uart": re.compile(r"esphome::uart::"),
    "dallas": re.compile(r"esphome::dallas::"),
    "dht": re.compile(r"esphome::dht::"),
    "adc": re.compile(r"esphome::adc::"),
    "pwm": re.compile(r"esphome::pwm::"),
    "ledc": re.compile(r"esphome::ledc::"),
    "gpio": re.compile(r"esphome::gpio::"),
    "esp32": re.compile(r"esphome::esp32::"),
    "esp8266": re.compile(r"esphome::esp8266::"),
    "remote": re.compile(r"esphome::remote_"),
    "rf_bridge": re.compile(r"esphome::rf_bridge::"),
    "captive_portal": re.compile(r"esphome::captive_portal::"),
    "deep_sleep": re.compile(r"esphome::deep_sleep::"),
    "bluetooth_proxy": re.compile(r"esphome::bluetooth_proxy::"),
    "esp32_ble": re.compile(r"esphome::esp32_ble::"),
    "esp32_ble_tracker": re.compile(r"esphome::esp32_ble_tracker::"),
    "ethernet": re.compile(r"esphome::ethernet::"),
    "core": re.compile(
        r"esphome::(?!api::|wifi::|mqtt::|web_server::|sensor::|binary_sensor::|switch_::|light::|cover::|climate::|fan::|display::|logger::|ota::|time::|sun::|text_sensor::|script::|interval::|json::|network::|mdns::|i2c::|spi::|uart::|dallas::|dht::|adc::|pwm::|ledc::|gpio::|esp32::|esp8266::|remote_|rf_bridge::|captive_portal::|deep_sleep::|bluetooth_proxy::|esp32_ble::|esp32_ble_tracker::|ethernet::)"
    ),
}


class MemorySection:
    """Represents a memory section with its symbols."""

    def __init__(self, name: str):
        self.name = name
        self.symbols: list[tuple[str, int, str]] = []  # (symbol_name, size, component)
        self.total_size = 0


class ComponentMemory:
    """Tracks memory usage for a component."""

    def __init__(self, name: str):
        self.name = name
        self.text_size = 0  # Code in flash
        self.rodata_size = 0  # Read-only data in flash
        self.data_size = 0  # Initialized data (flash + ram)
        self.bss_size = 0  # Uninitialized data (ram only)
        self.symbol_count = 0

    @property
    def flash_total(self) -> int:
        return self.text_size + self.rodata_size + self.data_size

    @property
    def ram_total(self) -> int:
        return self.data_size + self.bss_size


class MemoryAnalyzer:
    """Analyzes memory usage from ELF files."""

    def __init__(
        self,
        elf_path: str,
        objdump_path: str | None = None,
        readelf_path: str | None = None,
    ):
        self.elf_path = Path(elf_path)
        if not self.elf_path.exists():
            raise FileNotFoundError(f"ELF file not found: {elf_path}")

        self.objdump_path = objdump_path or "objdump"
        self.readelf_path = readelf_path or "readelf"

        self.sections: dict[str, MemorySection] = {}
        self.components: dict[str, ComponentMemory] = defaultdict(
            lambda: ComponentMemory("")
        )
        self._demangle_cache: dict[str, str] = {}

    def analyze(self) -> dict[str, ComponentMemory]:
        """Analyze the ELF file and return component memory usage."""
        self._parse_sections()
        self._parse_symbols()
        self._categorize_symbols()
        return dict(self.components)

    def _parse_sections(self):
        """Parse section headers from ELF file."""
        try:
            result = subprocess.run(
                [self.readelf_path, "-S", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            # Parse section headers
            for line in result.stdout.splitlines():
                # Look for section entries
                match = re.match(
                    r"\s*\[\s*\d+\]\s+([\.\w]+)\s+\w+\s+[\da-fA-F]+\s+[\da-fA-F]+\s+([\da-fA-F]+)",
                    line,
                )
                if match:
                    section_name = match.group(1)
                    size_hex = match.group(2)
                    size = int(size_hex, 16)

                    # Map various section names to standard categories
                    mapped_section = None
                    if ".text" in section_name or ".iram" in section_name:
                        mapped_section = ".text"
                    elif ".rodata" in section_name:
                        mapped_section = ".rodata"
                    elif ".data" in section_name and "bss" not in section_name:
                        mapped_section = ".data"
                    elif ".bss" in section_name:
                        mapped_section = ".bss"

                    if mapped_section:
                        if mapped_section not in self.sections:
                            self.sections[mapped_section] = MemorySection(
                                mapped_section
                            )
                        self.sections[mapped_section].total_size += size

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse sections: {e}")
            raise

    def _parse_symbols(self):
        """Parse symbols from ELF file."""
        try:
            result = subprocess.run(
                [self.objdump_path, "-t", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            for line in result.stdout.splitlines():
                # Parse symbol table entries
                # Format: address l/g w/d F/O section size name
                # Example: 40084870 l     F .iram0.text	00000000 _xt_user_exc
                parts = line.split()
                if len(parts) >= 5:
                    try:
                        # Check if this looks like a symbol entry
                        int(parts[0], 16)

                        # Look for F (function) or O (object) flag
                        if "F" in parts or "O" in parts:
                            # Find the section name
                            section = None
                            size = 0
                            name = None

                            for i, part in enumerate(parts):
                                if part.startswith("."):
                                    # Map section names
                                    if ".text" in part or ".iram" in part:
                                        section = ".text"
                                    elif ".rodata" in part:
                                        section = ".rodata"
                                    elif ".data" in part or ".dram" in part:
                                        section = ".data"
                                    elif ".bss" in part:
                                        section = ".bss"

                                    if section and i + 1 < len(parts):
                                        try:
                                            # Next field should be size
                                            size = int(parts[i + 1], 16)
                                            # Rest is the symbol name
                                            if i + 2 < len(parts):
                                                name = " ".join(parts[i + 2 :])
                                        except ValueError:
                                            pass
                                    break

                            if section and name and size > 0:
                                if section in self.sections:
                                    self.sections[section].symbols.append(
                                        (name, size, "")
                                    )

                    except ValueError:
                        # Not a valid address, skip
                        continue

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse symbols: {e}")
            raise

    def _categorize_symbols(self):
        """Categorize symbols by component."""
        # First, collect all unique symbol names for batch demangling
        all_symbols = set()
        for section in self.sections.values():
            for symbol_name, _, _ in section.symbols:
                all_symbols.add(symbol_name)

        # Batch demangle all symbols at once
        self._batch_demangle_symbols(list(all_symbols))

        # Now categorize with cached demangled names
        for section_name, section in self.sections.items():
            for symbol_name, size, _ in section.symbols:
                component = self._identify_component(symbol_name)

                if component not in self.components:
                    self.components[component] = ComponentMemory(component)

                comp_mem = self.components[component]
                comp_mem.symbol_count += 1

                if section_name == ".text":
                    comp_mem.text_size += size
                elif section_name == ".rodata":
                    comp_mem.rodata_size += size
                elif section_name == ".data":
                    comp_mem.data_size += size
                elif section_name == ".bss":
                    comp_mem.bss_size += size

    def _identify_component(self, symbol_name: str) -> str:
        """Identify which component a symbol belongs to."""
        # Demangle C++ names if needed
        demangled = self._demangle_symbol(symbol_name)

        # Check against component patterns
        for component, pattern in COMPONENT_PATTERNS.items():
            if pattern.search(demangled):
                return f"[esphome]{component}"

        # Check for web server related code
        if (
            "AsyncWebServer" in demangled
            or "AsyncWebHandler" in demangled
            or "WebServer" in demangled
        ):
            return "web_server_lib"
        elif "AsyncClient" in demangled or "AsyncServer" in demangled:
            return "async_tcp"

        # Check for FreeRTOS/ESP-IDF components
        if any(
            prefix in symbol_name
            for prefix in [
                "vTask",
                "xTask",
                "xQueue",
                "pvPort",
                "vPort",
                "uxTask",
                "pcTask",
            ]
        ):
            return "freertos"
        elif "xt_" in symbol_name or "_xt_" in symbol_name:
            return "xtensa"
        elif "heap_" in symbol_name or "multi_heap" in demangled:
            return "heap"
        elif "spi_flash" in symbol_name:
            return "spi_flash"
        elif "rtc_" in symbol_name:
            return "rtc"
        elif "gpio_" in symbol_name or "GPIO" in demangled:
            return "gpio_driver"
        elif "uart_" in symbol_name or "UART" in demangled:
            return "uart_driver"
        elif "timer_" in symbol_name or "esp_timer" in symbol_name:
            return "timer"
        elif "periph_" in symbol_name:
            return "peripherals"

        # C++ standard library
        if any(ns in demangled for ns in ["std::", "__gnu_cxx::", "__cxxabiv"]):
            return "cpp_stdlib"
        elif "_GLOBAL__N_" in symbol_name:
            return "cpp_anonymous"

        # Platform/system code
        if "esp_" in demangled or "ESP" in demangled:
            return "esp_system"
        elif "app_" in symbol_name:
            return "app_framework"
        elif "arduino" in demangled.lower():
            return "arduino"

        # Network stack components
        if any(
            net in demangled
            for net in [
                "lwip",
                "tcp",
                "udp",
                "ip4",
                "ip6",
                "dhcp",
                "dns",
                "netif",
                "ethernet",
                "ppp",
                "slip",
            ]
        ):
            return "network_stack"
        elif "vj_compress" in symbol_name:  # Van Jacobson TCP compression
            return "network_stack"

        # WiFi/802.11 stack
        if any(
            wifi in symbol_name
            for wifi in [
                "ieee80211",
                "hostap",
                "sta_",
                "ap_",
                "scan_",
                "wifi_",
                "wpa_",
                "wps_",
                "esp_wifi",
            ]
        ):
            return "wifi_stack"
        elif "NetworkInterface" in demangled:
            return "wifi_stack"

        # mDNS specific
        if (
            "mdns" in symbol_name or "mdns" in demangled
        ) and "esphome" not in demangled:
            return "mdns_lib"

        # Cryptography
        if any(
            crypto in demangled
            for crypto in [
                "mbedtls",
                "crypto",
                "sha",
                "aes",
                "rsa",
                "ecc",
                "tls",
                "ssl",
            ]
        ):
            return "crypto"

        # C library functions
        if any(
            libc in symbol_name
            for libc in [
                "printf",
                "scanf",
                "malloc",
                "free",
                "memcpy",
                "memset",
                "strcpy",
                "strlen",
                "_dtoa",
                "_fopen",
            ]
        ):
            return "libc"
        elif symbol_name.startswith("_") and symbol_name[1:].replace("_r", "").replace(
            "v", ""
        ).replace("s", "") in ["printf", "fprintf", "sprintf", "scanf"]:
            return "libc"

        # IPv6 specific
        if "nd6_" in symbol_name or "ip6_" in symbol_name:
            return "ipv6_stack"

        # Other system libraries
        if "nvs_" in demangled:
            return "nvs"
        elif "spiffs" in demangled or "vfs" in demangled:
            return "filesystem"
        elif "newlib" in demangled:
            return "libc"
        elif (
            "libgcc" in demangled
            or "_divdi3" in symbol_name
            or "_udivdi3" in symbol_name
        ):
            return "libgcc"

        # Boot and startup
        if any(
            boot in symbol_name
            for boot in ["boot", "start_cpu", "call_start", "startup", "bootloader"]
        ):
            return "boot_startup"

        # PHY/Radio layer
        if any(
            phy in symbol_name
            for phy in [
                "phy_",
                "rf_",
                "chip_",
                "register_chipv7",
                "pbus_",
                "bb_",
                "fe_",
            ]
        ):
            return "phy_radio"
        elif any(pp in symbol_name for pp in ["pp_", "ppT", "ppR", "ppP", "ppInstall"]):
            return "wifi_phy_pp"
        elif "lmac" in symbol_name:
            return "wifi_lmac"
        elif "wdev" in symbol_name:
            return "wifi_device"

        # Bluetooth/BLE
        if any(
            bt in symbol_name for bt in ["bt_", "ble_", "l2c_", "gatt_", "gap_", "hci_"]
        ):
            return "bluetooth"
        elif "coex" in symbol_name:
            return "wifi_bt_coex"

        # Power management
        if any(
            pm in symbol_name
            for pm in [
                "pm_",
                "sleep",
                "rtc_sleep",
                "light_sleep",
                "deep_sleep",
                "power_down",
            ]
        ):
            return "power_mgmt"

        # Logging and diagnostics
        if any(log in demangled for log in ["log", "Log", "print", "Print", "diag_"]):
            return "logging"

        # Memory management
        if any(mem in symbol_name for mem in ["mem_", "memory_", "tlsf_", "memp_"]):
            return "memory_mgmt"

        # HAL (Hardware Abstraction Layer)
        if "hal_" in symbol_name:
            return "hal_layer"

        # Clock management
        if any(
            clk in symbol_name
            for clk in ["clk_", "clock_", "rtc_clk", "apb_", "cpu_freq"]
        ):
            return "clock_mgmt"

        # Cache management
        if "cache" in symbol_name:
            return "cache_mgmt"

        # Flash operations
        if "flash" in symbol_name and "spi" not in symbol_name:
            return "flash_ops"

        # Interrupt/Exception handling
        if any(
            isr in symbol_name
            for isr in ["isr", "interrupt", "intr_", "exc_", "exception"]
        ):
            return "interrupt_handlers"
        elif "_wrapper" in symbol_name:
            return "wrapper_functions"

        # Error handling
        if any(
            err in symbol_name
            for err in ["panic", "abort", "assert", "error_", "fault"]
        ):
            return "error_handling"

        # ECC/Crypto math
        if any(
            ecc in symbol_name for ecc in ["ecp_", "bignum_", "mpi_", "sswu", "modp"]
        ):
            return "crypto_math"

        # Authentication
        if "checkDigestAuthentication" in demangled or "auth" in symbol_name.lower():
            return "authentication"

        # PPP protocol
        if any(ppp in symbol_name for ppp in ["ppp", "ipcp_", "lcp_", "chap_"]):
            return "ppp_protocol"

        # DHCP
        if "dhcp" in symbol_name or "handle_dhcp" in symbol_name:
            return "dhcp"

        return "other"

    def _batch_demangle_symbols(self, symbols: list[str]) -> None:
        """Batch demangle C++ symbol names for efficiency."""
        if not symbols:
            return

        try:
            # Send all symbols to c++filt at once
            result = subprocess.run(
                ["c++filt"],
                input="\n".join(symbols),
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0:
                demangled_lines = result.stdout.strip().split("\n")
                # Map original to demangled names
                for original, demangled in zip(symbols, demangled_lines):
                    self._demangle_cache[original] = demangled
            else:
                # If batch fails, cache originals
                for symbol in symbols:
                    self._demangle_cache[symbol] = symbol
        except Exception:
            # On error, cache originals
            for symbol in symbols:
                self._demangle_cache[symbol] = symbol

    def _demangle_symbol(self, symbol: str) -> str:
        """Get demangled C++ symbol name from cache."""
        return self._demangle_cache.get(symbol, symbol)

    def generate_report(self, detailed: bool = False) -> str:
        """Generate a formatted memory report."""
        components = sorted(
            self.components.items(), key=lambda x: x[1].flash_total, reverse=True
        )

        # Calculate totals
        total_flash = sum(c.flash_total for _, c in components)
        total_ram = sum(c.ram_total for _, c in components)

        # Build report
        lines = []
        lines.append("=" * 108)
        lines.append("                                  Component Memory Analysis")
        lines.append("=" * 108)
        lines.append("")

        # Main table
        lines.append(
            f"{'Component':<28} | {'Flash (text)':<12} | {'Flash (data)':<12} | {'RAM (data)':<10} | {'RAM (bss)':<10} | {'Total Flash':<12} | {'Total RAM':<10}"
        )
        lines.append(
            "-" * 28
            + "-+-"
            + "-" * 12
            + "-+-"
            + "-" * 12
            + "-+-"
            + "-" * 10
            + "-+-"
            + "-" * 10
            + "-+-"
            + "-" * 12
            + "-+-"
            + "-" * 10
        )

        for name, mem in components:
            if mem.flash_total > 0 or mem.ram_total > 0:
                flash_rodata = mem.rodata_size + mem.data_size
                lines.append(
                    f"{name:<28} | {mem.text_size:>11,} B | {flash_rodata:>11,} B | "
                    f"{mem.data_size:>9,} B | {mem.bss_size:>9,} B | "
                    f"{mem.flash_total:>11,} B | {mem.ram_total:>9,} B"
                )

        lines.append(
            "-" * 28
            + "-+-"
            + "-" * 12
            + "-+-"
            + "-" * 12
            + "-+-"
            + "-" * 10
            + "-+-"
            + "-" * 10
            + "-+-"
            + "-" * 12
            + "-+-"
            + "-" * 10
        )
        lines.append(
            f"{'TOTAL':<28} | {' ':>11} | {' ':>11} | "
            f"{' ':>9} | {' ':>9} | "
            f"{total_flash:>11,} B | {total_ram:>9,} B"
        )

        # Top consumers
        lines.append("")
        lines.append("Top Flash Consumers:")
        for i, (name, mem) in enumerate(components[:10]):
            if mem.flash_total > 0:
                percentage = (
                    (mem.flash_total / total_flash * 100) if total_flash > 0 else 0
                )
                lines.append(
                    f"{i + 1}. {name} ({mem.flash_total:,} B) - {percentage:.1f}% of analyzed flash"
                )

        lines.append("")
        lines.append("Top RAM Consumers:")
        ram_components = sorted(components, key=lambda x: x[1].ram_total, reverse=True)
        for i, (name, mem) in enumerate(ram_components[:10]):
            if mem.ram_total > 0:
                percentage = (mem.ram_total / total_ram * 100) if total_ram > 0 else 0
                lines.append(
                    f"{i + 1}. {name} ({mem.ram_total:,} B) - {percentage:.1f}% of analyzed RAM"
                )

        lines.append("")
        lines.append(
            "Note: This analysis covers symbols in the ELF file. Some runtime allocations may not be included."
        )
        lines.append("=" * 108)

        return "\n".join(lines)

    def to_json(self) -> str:
        """Export analysis results as JSON."""
        data = {
            "components": {
                name: {
                    "text": mem.text_size,
                    "rodata": mem.rodata_size,
                    "data": mem.data_size,
                    "bss": mem.bss_size,
                    "flash_total": mem.flash_total,
                    "ram_total": mem.ram_total,
                    "symbol_count": mem.symbol_count,
                }
                for name, mem in self.components.items()
            },
            "totals": {
                "flash": sum(c.flash_total for c in self.components.values()),
                "ram": sum(c.ram_total for c in self.components.values()),
            },
        }
        return json.dumps(data, indent=2)


def analyze_elf(
    elf_path: str,
    objdump_path: str | None = None,
    readelf_path: str | None = None,
    detailed: bool = False,
) -> str:
    """Analyze an ELF file and return a memory report."""
    analyzer = MemoryAnalyzer(elf_path, objdump_path, readelf_path)
    analyzer.analyze()
    return analyzer.generate_report(detailed)


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: analyze_memory.py <elf_file>")
        sys.exit(1)

    try:
        report = analyze_elf(sys.argv[1])
        print(report)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
