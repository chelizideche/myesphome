"""Memory usage analyzer for ESPHome compiled binaries."""

from collections import defaultdict
import json
import logging
from pathlib import Path
import re
import subprocess

_LOGGER = logging.getLogger(__name__)

# Pattern to extract ESPHome component namespaces dynamically
ESPHOME_COMPONENT_PATTERN = re.compile(r"esphome::([a-zA-Z0-9_]+)::")

# Component identification rules
# Symbol patterns: patterns found in raw symbol names
SYMBOL_PATTERNS = {
    "freertos": [
        "vTask",
        "xTask",
        "xQueue",
        "pvPort",
        "vPort",
        "uxTask",
        "pcTask",
        "prvTimerTask",
        "prvAddNewTaskToReadyList",
        "pxReadyTasksLists",
        "prvAddCurrentTaskToDelayedList",
        "xEventGroupWaitBits",
        "xRingbufferSendFromISR",
        "prvSendItemDoneNoSplit",
        "prvReceiveGeneric",
        "prvSendAcquireGeneric",
        "prvCopyItemAllowSplit",
        "xEventGroup",
        "xRingbuffer",
        "prvSend",
        "prvReceive",
        "prvCopy",
        "xPort",
        "ulTaskGenericNotifyTake",
        "prvIdleTask",
        "prvInitialiseNewTask",
        "prvIsYieldRequiredSMP",
        "prvGetItemByteBuf",
        "prvInitializeNewRingbuffer",
        "prvAcquireItemNoSplit",
        "prvNotifyQueueSetContainer",
        "ucStaticTimerQueueStorage",
    ],
    "xtensa": ["xt_", "_xt_", "xPortEnterCriticalTimeout"],
    "heap": ["heap_", "multi_heap"],
    "spi_flash": ["spi_flash"],
    "rtc": ["rtc_"],
    "gpio_driver": ["gpio_", "pins"],
    "uart_driver": ["uart", "_uart", "UART"],
    "timer": ["timer_", "esp_timer"],
    "peripherals": ["periph_", "periman"],
    "network_stack": [
        "vj_compress",
        "raw_sendto",
        "raw_input",
        "etharp_",
        "icmp_input",
        "socket_ipv6",
        "ip_napt",
        "socket_ipv4_multicast",
        "socket_ipv6_multicast",
        "netconn_",
        "recv_raw",
        "accept_function",
        "netconn_recv_data",
        "netconn_accept",
        "netconn_write_vectors_partly",
        "netconn_drain",
        "raw_connect",
        "raw_bind",
        "icmp_send_response",
        "sockets",
    ],
    "ipv6_stack": ["nd6_", "ip6_", "mld6_", "icmp6_", "icmp6_input"],
    "wifi_stack": [
        "ieee80211",
        "hostap",
        "sta_",
        "ap_",
        "scan_",
        "wifi_",
        "wpa_",
        "wps_",
        "esp_wifi",
        "cnx_",
        "wpa3_",
        "sae_",
        "wDev_",
        "ic_",
        "mac_",
        "esf_buf",
        "gWpaSm",
        "sm_WPA",
        "eapol_",
        "owe_",
        "wifiLowLevelInit",
        "s_do_mapping",
        "gScanStruct",
        "ppSearchTxframe",
        "ppMapWaitTxq",
        "ppFillAMPDUBar",
        "ppCheckTxConnTrafficIdle",
        "ppCalTkipMic",
    ],
    "bluetooth": ["bt_", "ble_", "l2c_", "gatt_", "gap_", "hci_", "BT_init"],
    "wifi_bt_coex": ["coex"],
    "bluetooth_rom": ["r_ble", "r_lld", "r_llc", "r_llm"],
    "bluedroid_bt": ["bluedroid", "btc_", "bta_", "btm_", "btu_"],
    "crypto_math": [
        "ecp_",
        "bignum_",
        "mpi_",
        "sswu",
        "modp",
        "dragonfly_",
        "gcm_mult",
        "__multiply",
        "quorem",
        "__mdiff",
        "__lshift",
        "__mprec_tens",
    ],
    "hw_crypto": ["esp_aes", "esp_sha", "esp_rsa", "esp_bignum", "esp_mpi"],
    "libc": [
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
        "__sfvwrite_r",
        "qsort",
        "__sf",
        "__sflush_r",
        "__srefill_r",
        "_impure_data",
        "_reclaim_reent",
        "_open_r",
        "strncpy",
    ],
    "string_ops": ["strcmp", "strncmp", "strchr", "strstr", "strtok", "strdup"],
    "memory_alloc": ["malloc", "calloc", "realloc", "free", "_sbrk"],
    "file_io": [
        "fread",
        "fwrite",
        "fopen",
        "fclose",
        "fseek",
        "ftell",
        "fflush",
        "s_fd_table",
    ],
    "string_formatting": [
        "snprintf",
        "vsnprintf",
        "sprintf",
        "vsprintf",
        "sscanf",
        "vsscanf",
    ],
    "cpp_anonymous": ["_GLOBAL__N_", "n$"],
    "cpp_runtime": ["__cxx", "_ZN", "_ZL", "_ZSt", "__gxx_personality", "_Z16"],
    "exception_handling": ["__cxa_", "_Unwind_", "__gcc_personality", "uw_frame_state"],
    "static_init": ["_GLOBAL__sub_I_"],
    "mdns_lib": ["mdns"],
    "phy_radio": [
        "phy_",
        "rf_",
        "chip_",
        "register_chipv7",
        "pbus_",
        "bb_",
        "fe_",
        "rfcal_",
        "ram_rfcal",
        "tx_pwctrl",
        "rx_chan",
        "set_rx_gain",
        "set_chan",
        "agc_reg",
        "ram_txiq",
        "ram_txdc",
        "ram_gen_rx_gain",
        "rx_11b_opt",
        "set_rx_sense",
        "set_rx_gain_cal",
        "set_chan_dig_gain",
        "tx_pwctrl_init_cal",
        "rfcal_txiq",
        "set_tx_gain_table",
        "correct_rfpll_offset",
        "pll_correct_dcap",
        "txiq_cal_init",
        "pwdet_sar",
        "pwdet_sar2_init",
    ],
    "wifi_phy_pp": ["pp_", "ppT", "ppR", "ppP", "ppInstall", "ppCalTxAMPDULength"],
    "wifi_lmac": ["lmac"],
    "wifi_device": ["wdev", "wDev_"],
    "power_mgmt": [
        "pm_",
        "sleep",
        "rtc_sleep",
        "light_sleep",
        "deep_sleep",
        "power_down",
        "g_pm",
    ],
    "memory_mgmt": [
        "mem_",
        "memory_",
        "tlsf_",
        "memp_",
        "pbuf_",
        "pbuf_alloc",
        "pbuf_copy_partial_pbuf",
    ],
    "hal_layer": ["hal_"],
    "clock_mgmt": [
        "clk_",
        "clock_",
        "rtc_clk",
        "apb_",
        "cpu_freq",
        "setCpuFrequencyMhz",
    ],
    "cache_mgmt": ["cache"],
    "flash_ops": ["flash", "image_load"],
    "interrupt_handlers": [
        "isr",
        "interrupt",
        "intr_",
        "exc_",
        "exception",
        "port_IntStack",
    ],
    "wrapper_functions": ["_wrapper"],
    "error_handling": ["panic", "abort", "assert", "error_", "fault"],
    "authentication": ["auth"],
    "ppp_protocol": ["ppp", "ipcp_", "lcp_", "chap_", "LcpEchoCheck"],
    "dhcp": ["dhcp", "handle_dhcp"],
    "ethernet_phy": [
        "emac_",
        "eth_phy_",
        "phy_tlk110",
        "phy_lan87",
        "phy_ip101",
        "phy_rtl",
        "phy_dp83",
        "phy_ksz",
    ],
    "threading": ["pthread_", "thread_", "_task_"],
    "pthread": ["pthread"],
    "synchronization": ["mutex", "semaphore", "spinlock", "portMUX"],
    "math_lib": [
        "sin",
        "cos",
        "tan",
        "sqrt",
        "pow",
        "exp",
        "log",
        "atan",
        "asin",
        "acos",
        "floor",
        "ceil",
        "fabs",
        "round",
    ],
    "random": ["rand", "random", "rng_", "prng"],
    "time_lib": [
        "time",
        "clock",
        "gettimeofday",
        "settimeofday",
        "localtime",
        "gmtime",
        "mktime",
        "strftime",
    ],
    "console_io": ["console_", "uart_tx", "uart_rx", "puts", "putchar", "getchar"],
    "rom_functions": ["r_", "rom_"],
    "compiler_runtime": [
        "__divdi3",
        "__udivdi3",
        "__moddi3",
        "__muldi3",
        "__ashldi3",
        "__ashrdi3",
        "__lshrdi3",
        "__cmpdi2",
        "__fixdfdi",
        "__floatdidf",
    ],
    "libgcc": ["libgcc", "_divdi3", "_udivdi3"],
    "boot_startup": ["boot", "start_cpu", "call_start", "startup", "bootloader"],
    "bootloader": ["bootloader_", "esp_bootloader"],
    "app_framework": ["app_", "initArduino", "setup", "loop", "Update"],
    "weak_symbols": ["__weak_"],
    "compiler_builtins": ["__builtin_"],
    "vfs": ["vfs_", "VFS"],
    "esp32_sdk": ["esp32_", "esp32c", "esp32s"],
    "usb": ["usb_", "USB", "cdc_", "CDC"],
    "i2c_driver": ["i2c_", "I2C"],
    "i2s_driver": ["i2s_", "I2S"],
    "spi_driver": ["spi_", "SPI"],
    "adc_driver": ["adc_", "ADC"],
    "dac_driver": ["dac_", "DAC"],
    "touch_driver": ["touch_", "TOUCH"],
    "pwm_driver": ["pwm_", "PWM", "ledc_", "LEDC"],
    "rmt_driver": ["rmt_", "RMT"],
    "pcnt_driver": ["pcnt_", "PCNT"],
    "can_driver": ["can_", "CAN", "twai_", "TWAI"],
    "sdmmc_driver": ["sdmmc_", "SDMMC", "sdcard", "sd_card"],
    "temp_sensor": ["temp_sensor", "tsens_"],
    "watchdog": ["wdt_", "WDT", "watchdog"],
    "brownout": ["brownout", "bod_"],
    "ulp": ["ulp_", "ULP"],
    "psram": ["psram", "PSRAM", "spiram", "SPIRAM"],
    "efuse": ["efuse", "EFUSE"],
    "partition": ["partition", "esp_partition"],
    "esp_event": ["esp_event", "event_loop", "event_callback"],
    "esp_console": ["esp_console", "console_"],
    "chip_specific": ["chip_", "esp_chip"],
    "esp_system_utils": ["esp_system", "esp_hw", "esp_clk", "esp_sleep"],
    "ipc": ["esp_ipc", "ipc_"],
    "wifi_config": [
        "g_cnxMgr",
        "gChmCxt",
        "g_ic",
        "TxRxCxt",
        "s_dp",
        "s_ni",
        "s_reg_dump",
        "packet$",
        "d_mult_table",
        "K",
        "fcstab",
    ],
    "smartconfig": ["sc_ack_send"],
    "rc_calibration": ["rc_cal", "rcUpdate"],
    "noise_floor": ["noise_check"],
    "rf_calibration": [
        "set_rx_sense",
        "set_rx_gain_cal",
        "set_chan_dig_gain",
        "tx_pwctrl_init_cal",
        "rfcal_txiq",
        "set_tx_gain_table",
        "correct_rfpll_offset",
        "pll_correct_dcap",
        "txiq_cal_init",
        "pwdet_sar",
        "rx_11b_opt",
    ],
    "wifi_crypto": [
        "pk_use_ecparams",
        "process_segments",
        "ccmp_",
        "rc4_",
        "aria_",
        "mgf_mask",
        "dh_group",
        "ccmp_aad_nonce",
        "ccmp_encrypt",
        "rc4_skip",
        "aria_sb1",
        "aria_sb2",
        "aria_is1",
        "aria_is2",
        "aria_sl",
        "aria_a",
    ],
    "radio_control": ["fsm_input", "fsm_sconfreq"],
    "pbuf": [
        "pbuf_",
    ],
    "event_group": ["xEventGroup"],
    "ringbuffer": ["xRingbuffer", "prvSend", "prvReceive", "prvCopy"],
    "provisioning": ["prov_", "prov_stop_and_notify"],
    "scan": ["gScanStruct"],
    "port": ["xPort"],
    "elf_loader": [
        "elf_add",
        "elf_add_note",
        "elf_add_segment",
        "process_image",
        "read_encoded",
        "read_encoded_value",
        "read_encoded_value_with_base",
        "process_image_header",
    ],
    "socket_api": [
        "sockets",
        "netconn_",
        "accept_function",
        "recv_raw",
        "socket_ipv4_multicast",
        "socket_ipv6_multicast",
    ],
    "igmp": ["igmp_", "igmp_send", "igmp_input"],
    "icmp6": ["icmp6_"],
    "arp": ["arp_table"],
    "ampdu": [
        "ampdu_",
        "rcAmpdu",
        "trc_onAmpduOp",
        "rcAmpduLowerRate",
        "ampdu_dispatch_upto",
    ],
    "ieee802_11": ["ieee802_11_", "ieee802_11_parse_elems"],
    "rate_control": ["rssi_margin", "rcGetSched", "get_rate_fcc_index"],
    "nan": ["nan_dp_", "nan_dp_post_tx", "nan_dp_delete_peer"],
    "channel_mgmt": ["chm_init", "chm_set_current_channel"],
    "trace": ["trc_init", "trc_onAmpduOp"],
    "country_code": ["country_info", "country_info_24ghz"],
    "multicore": ["do_multicore_settings"],
    "Update_lib": ["Update"],
    "stdio": [
        "__sf",
        "__sflush_r",
        "__srefill_r",
        "_impure_data",
        "_reclaim_reent",
        "_open_r",
    ],
    "strncpy_ops": ["strncpy"],
    "math_internal": ["__mdiff", "__lshift", "__mprec_tens", "quorem"],
    "character_class": ["__chclass"],
    "camellia": ["camellia_", "camellia_feistel"],
    "crypto_tables": ["FSb", "FSb2", "FSb3", "FSb4"],
    "event_buffer": ["g_eb_list_desc", "eb_space"],
    "base_node": ["base_node_", "base_node_add_handler"],
    "file_descriptor": ["s_fd_table"],
    "tx_delay": ["tx_delay_cfg"],
    "deinit": ["deinit_functions"],
    "lcp_echo": ["LcpEchoCheck"],
    "raw_api": ["raw_bind", "raw_connect"],
    "checksum": ["process_checksum"],
    "entry_management": ["add_entry"],
    "esp_ota": ["esp_ota", "ota_"],
}

# Demangled patterns: patterns found in demangled C++ names
DEMANGLED_PATTERNS = {
    "gpio_driver": ["GPIO"],
    "uart_driver": ["UART"],
    "network_stack": [
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
    ],
    "wifi_stack": ["NetworkInterface"],
    "nimble_bt": [
        "nimble",
        "NimBLE",
        "ble_hs",
        "ble_gap",
        "ble_gatt",
        "ble_att",
        "ble_l2cap",
        "ble_sm",
    ],
    "crypto": ["mbedtls", "crypto", "sha", "aes", "rsa", "ecc", "tls", "ssl"],
    "cpp_stdlib": ["std::", "__gnu_cxx::", "__cxxabiv"],
    "static_init": ["__static_initialization"],
    "rtti": ["__type_info", "__class_type_info"],
    "web_server_lib": ["AsyncWebServer", "AsyncWebHandler", "WebServer"],
    "async_tcp": ["AsyncClient", "AsyncServer"],
    "mdns_lib": ["mdns"],
    "json_lib": [
        "ArduinoJson",
        "JsonDocument",
        "JsonArray",
        "JsonObject",
        "deserialize",
        "serialize",
    ],
    "http_lib": ["HTTP", "http_", "Request", "Response", "Uri", "WebSocket"],
    "logging": ["log", "Log", "print", "Print", "diag_"],
    "authentication": ["checkDigestAuthentication"],
    "libgcc": ["libgcc"],
    "esp_system": ["esp_", "ESP"],
    "arduino": ["arduino"],
    "nvs": ["nvs_"],
    "filesystem": ["spiffs", "vfs"],
    "libc": ["newlib"],
}


# Get the list of actual ESPHome components by scanning the components directory
def get_esphome_components():
    """Get set of actual ESPHome components from the components directory."""
    components = set()

    # Find the components directory relative to this file
    current_dir = Path(__file__).parent
    components_dir = current_dir / "components"

    if components_dir.exists() and components_dir.is_dir():
        for item in components_dir.iterdir():
            if (
                item.is_dir()
                and not item.name.startswith(".")
                and not item.name.startswith("__")
            ):
                components.add(item.name)

    return components


# Cache the component list
ESPHOME_COMPONENTS = get_esphome_components()


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
        self._uncategorized_symbols: list[tuple[str, str, int]] = []

    def analyze(self) -> dict[str, ComponentMemory]:
        """Analyze the ELF file and return component memory usage."""
        self._parse_sections()
        self._parse_symbols()
        self._categorize_symbols()
        return dict(self.components)

    def _parse_sections(self) -> None:
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

    def _parse_symbols(self) -> None:
        """Parse symbols from ELF file."""
        # Section mapping - centralizes the logic
        SECTION_MAPPING = {
            ".text": [".text", ".iram"],
            ".rodata": [".rodata"],
            ".data": [".data", ".dram"],
            ".bss": [".bss"],
        }

        def map_section_name(raw_section: str) -> str | None:
            """Map raw section name to standard section."""
            for standard_section, patterns in SECTION_MAPPING.items():
                if any(pattern in raw_section for pattern in patterns):
                    return standard_section
            return None

        def parse_symbol_line(line: str) -> tuple[str, str, int] | None:
            """Parse a single symbol line from objdump output.

            Returns (section, name, size) or None if not a valid symbol.
            Format: address l/g w/d F/O section size name
            Example: 40084870 l     F .iram0.text    00000000 _xt_user_exc
            """
            parts = line.split()
            if len(parts) < 5:
                return None

            try:
                # Validate address
                int(parts[0], 16)
            except ValueError:
                return None

            # Look for F (function) or O (object) flag
            if "F" not in parts and "O" not in parts:
                return None

            # Find section, size, and name
            for i, part in enumerate(parts):
                if part.startswith("."):
                    section = map_section_name(part)
                    if section and i + 1 < len(parts):
                        try:
                            size = int(parts[i + 1], 16)
                            if i + 2 < len(parts) and size > 0:
                                name = " ".join(parts[i + 2 :])
                                return (section, name, size)
                        except ValueError:
                            pass
                    break
            return None

        try:
            result = subprocess.run(
                [self.objdump_path, "-t", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            for line in result.stdout.splitlines():
                symbol_info = parse_symbol_line(line)
                if symbol_info:
                    section, name, size = symbol_info
                    if section in self.sections:
                        self.sections[section].symbols.append((name, size, ""))

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse symbols: {e}")
            raise

    def _categorize_symbols(self) -> None:
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

                # Track uncategorized symbols
                if component == "other" and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._uncategorized_symbols.append((symbol_name, demangled, size))

    def _identify_component(self, symbol_name: str) -> str:
        """Identify which component a symbol belongs to."""
        # Demangle C++ names if needed
        demangled = self._demangle_symbol(symbol_name)

        # Check for ESPHome component namespaces first
        match = ESPHOME_COMPONENT_PATTERN.search(demangled)
        if match:
            component_name = match.group(1)
            # Strip trailing underscore if present (e.g., switch_ -> switch)
            component_name = component_name.rstrip("_")

            # Check if this is an actual component or core
            if component_name in ESPHOME_COMPONENTS:
                return f"[esphome]{component_name}"
            else:
                return "[esphome]core"

        # Check for esphome core namespace (no component namespace)
        if "esphome::" in demangled:
            return "[esphome]core"

        # Check against symbol patterns
        for component, patterns in SYMBOL_PATTERNS.items():
            if any(pattern in symbol_name for pattern in patterns):
                return component

        # Check against demangled patterns
        for component, patterns in DEMANGLED_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return component

        # Special cases that need more complex logic

        # Check if spi_flash vs spi_driver
        if "spi_" in symbol_name or "SPI" in symbol_name:
            if "spi_flash" in symbol_name:
                return "spi_flash"
            else:
                return "spi_driver"

        # libc special printf variants
        if symbol_name.startswith("_") and symbol_name[1:].replace("_r", "").replace(
            "v", ""
        ).replace("s", "") in ["printf", "fprintf", "sprintf", "scanf"]:
            return "libc"

        # Track uncategorized symbols for analysis
        return "other"

    def _batch_demangle_symbols(self, symbols: list[str]) -> None:
        """Batch demangle C++ symbol names for efficiency."""
        if not symbols:
            return

        # Try to find the appropriate c++filt for the platform
        cppfilt_cmd = "c++filt"

        # Check if we have a toolchain-specific c++filt
        if self.objdump_path and self.objdump_path != "objdump":
            # Replace objdump with c++filt in the path
            potential_cppfilt = self.objdump_path.replace("objdump", "c++filt")
            if Path(potential_cppfilt).exists():
                cppfilt_cmd = potential_cppfilt

        try:
            # Send all symbols to c++filt at once
            result = subprocess.run(
                [cppfilt_cmd],
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
            f"{'Component':<28} | {'Flash (text)':>12} | {'Flash (data)':>12} | {'RAM (data)':>10} | {'RAM (bss)':>10} | {'Total Flash':>12} | {'Total RAM':>10}"
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

    def dump_uncategorized_symbols(self, output_file: str | None = None) -> None:
        """Dump uncategorized symbols for analysis."""
        # Sort by size descending
        sorted_symbols = sorted(
            self._uncategorized_symbols, key=lambda x: x[2], reverse=True
        )

        lines = ["Uncategorized Symbols Analysis", "=" * 80]
        lines.append(f"Total uncategorized symbols: {len(sorted_symbols)}")
        lines.append(
            f"Total uncategorized size: {sum(s[2] for s in sorted_symbols):,} bytes"
        )
        lines.append("")
        lines.append(f"{'Size':>10} | {'Symbol':<60} | Demangled")
        lines.append("-" * 10 + "-+-" + "-" * 60 + "-+-" + "-" * 40)

        for symbol, demangled, size in sorted_symbols[:100]:  # Top 100
            if symbol != demangled:
                lines.append(f"{size:>10,} | {symbol[:60]:<60} | {demangled[:100]}")
            else:
                lines.append(f"{size:>10,} | {symbol[:60]:<60} | [not demangled]")

        if len(sorted_symbols) > 100:
            lines.append(f"\n... and {len(sorted_symbols) - 100} more symbols")

        content = "\n".join(lines)

        if output_file:
            with open(output_file, "w") as f:
                f.write(content)
        else:
            print(content)


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
