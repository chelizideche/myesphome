# OpenThread Zephyr Component for ESPHome

This component enables OpenThread networking support for nRF52 devices using the Zephyr RTOS in ESPHome.

## Overview

OpenThread is an open-source implementation of the Thread networking protocol. Thread is an IPv6-based mesh networking protocol designed for IoT devices, providing secure, reliable, and low-power wireless connectivity.

This component integrates OpenThread with ESPHome on nRF52 devices using the Zephyr RTOS, allowing your ESPHome devices to participate in Thread networks.

## Features

- Full Thread networking support
- IPv6 connectivity
- Service Registration Protocol (SRP) for service discovery
- mDNS integration for local service discovery
- Automatic network joining and reconnection
- Support for Thread operational datasets

## Requirements

- nRF52 device (e.g., nRF52840)
- Zephyr RTOS support
- ESPHome
- Thread border router (for connecting to other IP networks)

## Configuration

```yaml
# Example configuration
openthread_zephyr:
  channel: 15                                  # IEEE 802.15.4 channel (11-26)
  panid: 43981                                 # Personal Area Network ID
  network_name: "ESPHome-Thread"               # Thread network name
  xpanid: "FEDCBA9876543210"                   # Extended PAN ID (16 hex chars)
  network_key: "00112233445566778899AABBCCDDEEFF"  # Network key (32 hex chars)
  pskc: "00112233445566778899AABBCCDDEEFF"     # Pre-Shared Key for Commissioner (32 hex chars)
  radio_tx_power: 0                            # Radio TX power in dBm (-20 to 20)
  force_dataset: false                         # Force using operational dataset
```

## Configuration Variables

- **channel** (*Optional*, int): The IEEE 802.15.4 channel to use. Range is 11-26. Default is `15`.
- **panid** (*Optional*, hex): The Personal Area Network ID. Range is 0x0000-0xFFFF. Default is `0xABCD`.
- **network_name** (*Optional*, string): The Thread network name. Default is `ESPHome-OpenThread`.
- **xpanid** (*Optional*, hex string): The Extended PAN ID as a 16-character hex string. Default is `FEDCBA9876543210`.
- **network_key** (*Optional*, hex string): The Thread network key as a 32-character hex string. Default is `00112233445566778899AABBCCDDEEFF`.
- **pskc** (*Optional*, hex string): The Pre-Shared Key for Commissioner as a 32-character hex string. Default is `00112233445566778899AABBCCDDEEFF`.
- **radio_tx_power** (*Optional*, int): The radio transmission power in dBm. Range is -20 to 20. Default is `0`.
- **force_dataset** (*Optional*, boolean): Whether to force using the operational dataset. Default is `false`.

## IPv6 Support

To enable IPv6 support, add the following to your configuration:

```yaml
network:
  enable_ipv6: true
```

This will enable IPv6 networking for your ESPHome device, allowing it to communicate with other IPv6 devices on the Thread network.

## mDNS Support

The component automatically integrates with ESPHome's mDNS component to provide service discovery. To enable mDNS, add the following to your configuration:

```yaml
mdns:
```

This will enable mDNS service discovery, allowing your ESPHome device to be discovered by Home Assistant and other services on your network.

### How mDNS Works with Thread

In Thread networks, traditional mDNS multicast traffic is inefficient and can consume significant radio bandwidth. Instead, this component uses a bridge between ESPHome's mDNS component and OpenThread's Service Registration Protocol (SRP).

When you configure an mDNS service in ESPHome, the bridge automatically:

1. Intercepts the service registration
2. Forwards it to the Thread SRP client
3. The SRP client registers the service with a Thread SRP server (typically running on your Thread border router)
4. The SRP server makes the service discoverable both within the Thread network and to external networks

This approach is more efficient and conserves radio bandwidth while still providing the same service discovery functionality.

## Full Example

```yaml
esphome:
  name: openthread-nrf52-example
  friendly_name: OpenThread NRF52 Example

# Use nrf52 platform with zephyr framework
nrf52:
  board: adafruit_itsybitsy_nrf52840
  bootloader: mcuboot

# Enable logging
logger:
  level: DEBUG
  baud_rate: 115200

# Enable IPv6 networking
network:
  enable_ipv6: true

# Enable OpenThread
openthread_zephyr:
  channel: 15
  panid: 43981
  network_name: "ESPHome-Thread"
  xpanid: "FEDCBA9876543210"
  network_key: "00112233445566778899AABBCCDDEEFF"
  pskc: "00112233445566778899AABBCCDDEEFF"
  radio_tx_power: 0
  force_dataset: false

# Enable mDNS for service discovery
mdns:
  # mDNS services will be automatically bridged to Thread SRP

# Enable API for Home Assistant integration
api:
  encryption:
    key: "YourEncryptionKeyHere"

# Example sensor
sensor:
  - platform: uptime
    name: Uptime
    update_interval: 60s
```

## Troubleshooting

- Make sure your nRF52 device has the necessary hardware to support IEEE 802.15.4 (e.g., nRF52840).
- Ensure that all devices in your Thread network use the same channel, PAN ID, and network key.
- Check the logs for connection status and any error messages.
- If you're having trouble connecting to the Thread network, try increasing the radio TX power.
- Make sure you have a Thread border router set up if you want to communicate with devices outside the Thread network.
- If you're using Home Assistant, make sure your Thread border router is properly integrated with Home Assistant.

## Notes

- This component requires the Zephyr RTOS and will only work with nRF52 devices.
- Thread networks are separate from Wi-Fi networks, so your device will need a Thread border router to communicate with IP devices outside the Thread network.
- For security reasons, it's recommended to change the default network key and PSKC to unique values.
- The SRP client is used to register services with a Thread SRP server, which is typically running on the Thread border router.
- IPv6 addresses in Thread networks typically start with `fd` (mesh-local) or `2` (global). 