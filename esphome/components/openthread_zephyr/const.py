from typing import Final

import esphome.codegen as cg

# OpenThread constants
CONF_OPENTHREAD = "openthread"
CONF_CHANNEL = "channel"
CONF_PANID = "panid"
CONF_NETWORK_NAME = "network_name"
CONF_XPANID = "xpanid"
CONF_NETWORK_KEY = "network_key"
CONF_MESH_PREFIX = "mesh_prefix"
CONF_MESH_PASSWORD = "mesh_password"
CONF_DATASET_TIMESTAMP = "dataset_timestamp"
CONF_RADIO_TX_POWER = "radio_tx_power"
CONF_PSKC = "pskc"
CONF_FORCE_DATASET = "force_dataset"

# Default values
DEFAULT_CHANNEL = 15
DEFAULT_PANID = 0xABCD
DEFAULT_NETWORK_NAME = "ESPHome-OpenThread"
DEFAULT_XPANID = "FEDCBA9876543210"
DEFAULT_NETWORK_KEY = "00112233445566778899AABBCCDDEEFF"
DEFAULT_RADIO_TX_POWER = 0  # dBm
DEFAULT_PSKC = "00112233445566778899AABBCCDDEEFF"
DEFAULT_FORCE_DATASET = False

# Namespace
openthread_zephyr_ns = cg.esphome_ns.namespace("openthread_zephyr")
# Add a define to ensure the namespace is recognized