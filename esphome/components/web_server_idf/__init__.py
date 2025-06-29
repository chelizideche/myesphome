from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@dentra"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_esp_idf,
)


async def to_code(config):
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)

    # Check if web_server component has OTA enabled
    web_server_config = CORE.config.get("web_server", {})
    if web_server_config and web_server_config.get("ota", True):
        # Add multipart parser component for ESP-IDF OTA support
        add_idf_component(name="zorxx/multipart-parser", ref="1.0.1")
