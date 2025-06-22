
# ESPHome Hamulight RF Integration

[![ESPHome](https://img.shields.io/badge/ESPHome-Component-blue.svg)](https://esphome.io/)
[![Hamulight](https://img.shields.io/badge/Hamulight-RF-green.svg)](https://www.hamulight.com/)

**Control Hamulight RF lights and drivers with ESPHome on ESP32 (S2/S3/C3) using the RMT peripheral for precise RF signal generation.**

This project is in no way associated, supported or otherwise linked to **Hamulight B.V.** or any of its affiliates!

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [How the Hamulight RF Protocol Works](#how-the-hamulight-rf-protocol-works)
  - [Protocol Summary](#protocol-summary)
  - [RF Command Structure](#rf-command-structure)
  - [Timing and Transmission](#timing-and-transmission)
- [Hardware Requirements](#hardware-requirements)
- [Installation](#installation)
- [Commands / Constants](#commands--constants)
- [YAML Configuration](#yaml-configuration)
  - [Component Options](#component-options)
  - [Example Configuration](#example-configuration)
  - [Buttons](#buttons)
  - [Numbers](#numbers)
  - [Sensors](#sensors)
- [Home Assistant Integration](#home-assistant-integration)
- [License](#license)
- [Acknowledgements](#acknowledgements)
- [Pairing process](#pairing-process)

---

## Overview

This ESPHome custom component enables direct control of Hamulight RF-based lighting systems using an ESP32 (including S2/S3/C3 and other variants). It replays the proprietary Hamulight RF protocol using the ESP32's RMT peripheral for precise waveform generation. You can toggle lights on/off, pair with drivers (= max brightness button on the remote), set brightness — all from Home Assistant.
This component also includes an optional "command scan" feature which allows to batch-send commands within a defined range for finding _unknown_ commands (not populated on your remote).

The ESPHome Hamulight componenent is written as dynamical as possible to allow other devs to use it for other protocol implementations as well.

ESPHome Hamulight does **not** use HomeAssistant's light entities. Reason for that is that this project is following a stateless approach (as you never know, if a RF signal has been received and executed by the LED driver).
This makes the YAML configuration part a little bit more complex, but also more flexible at the same time.

---

## Features

- **Full control over Hamulight RF devices** (on/off, pairing, brightness, and command scanning)
- **Native Home Assistant integration** via ESPHome (`button` and `number` entities)
- **Command scanner** for discovering unknown commands
- **Highly customizable via YAML**

---

## How the Hamulight RF Protocol Works

### Protocol Summary

The Hamulight RF protocol is a proprietary 433MHz protocol used by Hamulight lighting products. This project reverse-engineers and replicates the protocol with high timing accuracy.

- **Frequency:** 433 MHz (OOK/ASK modulation)
- **Main control:** 32-bit address, 8-bit command, 8-bit checksum (total 4 bytes)
- **Each command is repeated multiple times for reliability**

### RF Command Structure

Each RF message (per transmission) consists of:

| Field           | Size (bits) | Description                    |
|-----------------|-------------|--------------------------------|
| Address High    | 8           | High byte of RF address        |
| Address Low     | 8           | Low byte of RF address         |
| Command         | 8           | Command byte (e.g., 0x5F=Toggle, 0x59=Pair) |
| Checksum        | 8           | addr_hi + addr_lo + cmd - 83   |

**Transmission sequence:**

1. **Start Sequence:** 10 pulses (8x[1,1]+2x[6,6])
2. **Data:** 32 bits (address, command, checksum), encoded as pairs of pulses:
    - **"0" bit:** [3,1] (high,low)
    - **"1" bit:** [1,3] (high,low)
3. **Repeat:** Full sequence transmitted 6 times per command

### Timing and Transmission

- **Base pulse:** 200 microseconds (us)
- **Start sequence:** 8 x [1,1] (200us high, 200us low), then 2 x [6,6] (1200us high, 1200us low)
- **Data bits:** Each bit is a [high,low] pair, either [3,1] (0) or [1,3] (1) in multiples of base pulse
- **Total bits:** 32 (address, command, checksum)

**Example:**
- Toggle command: Address = 0xC535, Command = 0x5F (Toggle), Checksum = 0x35+0xC5+0x5F-83

---

## Hardware Requirements

- **ESP32, ESP32-S2, ESP32-S3, or ESP32-C3** microcontroller
- **433MHz RF transmitter module** (connect to any suitable ESP32 GPIO)
- (Optional) **Status LED** for RF activity indication
- **Hamulight RF-based lighting driver(s)**

---

## Installation

1. **Add this repository as an external component** in your ESPHome YAML configuration:
   ```yaml
hamulight:
   ```
2. **Configure the `hamulight` component** (see examples below)
3. **Wire your 433MHz transmitter** to the selected GPIO on your ESP32
4. **Compile and upload** your ESPHome firmware

---
## Commands / Constants


**Address / Header** <br/>
First 2 bytes of the protocol is the remote's ID. The assumption (based on the the analysis of 2 different remotes) is that you can choose ANY address here, since the driver/receiver can be trained for any Hamulight remote with that protocol.

**Remote Control's Commands**<br/>
5 buttons of the remote are hard-coded, a capacitive touch field is used for seamless dimming

| Command         | Byte | Description                                                               |
| --------------- | ---- | ------------------------------------------------------------------------- |
| RFaddress       | var  | change the address in the YAML config as needed to match your physical remote                |
| RFpower         | `0x5F` | on/off toggle                                                             |
| RFbright100     | `0x59` | 100% brightness command + command for training the transformator/receiver |
| RFbright75      | `0x50` | 75% brightness command                                                    |
| RFbright50      |	`0x56` | 50% brightness command                                                    |
| RFbright25      |	`0x55` | 25% brightness command                                                    |
| RFslideRangeMin | `0x80` | lowest HEX value used by dimm slider                                      |
| RFslideRangeMax |	`0xFF` | highest HEX value used by dimm slider                                     |
| RFslideOffset   |	`0xA8` | needed for the offset in the slideValConv() function                      |
| RFslideSteps    | calc | steps from 0% to 100% dimm value (RFslideRangeMax - RFslideRangeMin + 1)  |
| RFslideStart    | calc | is the 0% dimm value position    (RFslideRangeMin + RFslideOffset)        |

---

## YAML Configuration

### Component Options

| Option              | Type      | Required | Description                               |
|---------------------|-----------|----------|-------------------------------------------|
| `id`                | string    | Yes      | ESPHome component ID                      |
| `rf_transmit_pin`   | GPIO      | Yes      | GPIO for 433MHz transmitter               |
| `rf_address`        | uint16    | Yes      | RF address (hex, unique per device/group) |
| `led_pin`           | GPIO      | No       | (Optional) Activity LED GPIO              |
| `command_scanner`   | map       | No       | (Optional) Command scan config            |

**Command scanner options:**

| Option                | Type    | Required | Description                               |
|-----------------------|---------|----------|-------------------------------------------|
| `enabled`             | bool    | No       | Enable command scanner (default: true)    |
| `cmdscan_start`       | id      | No       | ID of start value number entity           |
| `cmdscan_end`         | id      | No       | ID of end value number entity             |
| `cmdscan_pause`       | id      | No       | ID of pause duration number entity        |
| `last_scanned_sensor` | id      | No       | ID of sensor to publish last scanned cmd  |

---

### Example Configuration

```yaml
hamulight:
id: hamulight_transmitter
rf_transmit_pin: GPIO2
# led_pin: GPIO21     # <- optional: GPIO for feedback LED
rf_address: 0xC535    # <- your remote's unique ID, which the LED driver is / should be paired with
  
# Command scanner configuration (optional)
# To use, uncomment and define the matching number/sensor blocks below:
# command_scanner:
#   enabled: true
#   cmdscan_start: hamulight_cmdscan_start
#   cmdscan_end: hamulight_cmdscan_end
#   cmdscan_pause: hamulight_cmdscan_pause
#   last_scanned_sensor: hamulight_last_scanned_command

# IMPORTANT: The sensor component is required, even if the command scanner is not used
#            You can use e.g. the uptime-sensor as in the example below (or none at all)
sensor:				 
- platform: uptime
  name: "Uptime"
#- platform: template
#  id: hamulight_last_scanned_command
#  name: "Hamulight Last Scanned Command"

number:
- platform: template
  id: hamulight_brightness
  name: "Hamulight Brightness"
  min_value: 0
  max_value: 100
  step: 1
  optimistic: true
  initial_value: 100
  on_value:
  - lambda: |-
    id(hamulight_transmitter).set_brightness(x);

# OPTIONAL - Following number sensors are needed for the command scanner
# - platform: template
#   id: hamulight_cmdscan_start
#   name: "Command Scan Start"
#   min_value: 0
#   max_value: 127
#   step: 1
#   optimistic: true
#   initial_value: 0
# - platform: template
#   id: hamulight_cmdscan_end
#   name: "Command Scan End"
#   min_value: 0
#   max_value: 127
#   step: 1
#   optimistic: true
#   initial_value: 127
# - platform: template
#   id: hamulight_cmdscan_pause
#   name: "Command Scan Pause"
#   min_value: 0
#   max_value: 7000
#   step: 100
#   optimistic: true
#   initial_value: 500

  

button:
- platform: template
  name: "Toggle Hamulight"
  on_press:
  - lambda: |-
      id(hamulight_transmitter).toggle();
- platform: template
  name: "Pair with Driver"
on_press:
  - lambda: |-
      id(hamulight_transmitter).pair_with_driver();

# OPTIONAL - Following button sensors are needed for the command scanner
# - platform: template
#   name: "Start Command Scan"
#   on_press:
#     - lambda: |-
#       id(hamulight_transmitter).start_command_scan();
# - platform: template
#   name: "Stop Command Scan"
#     on_press:
#       - lambda: |-
#         id(hamulight_transmitter).stop_command_scan();
```

---

### Buttons

| Name                    | Description                                 | Calls C++ Method                      |
|-------------------------|---------------------------------------------|---------------------------------------|
| Toggle Hamulight        | Toggle the connected Hamulight device on/off       | `toggle()`                            |
| Pair with Driver        | Send max. brightness command = pairing the remote / ESP32 with Hamulight driver in pairing mode        | `pair_with_driver()`                  |
| Start Command Scan      | Start scanning a range of commands          | `start_command_scan()`                |
| Stop Command Scan       | Stop command scanning                       | `stop_command_scan()`                 |

---

### Numbers

| Name                   | Description                                 | Used for Command Scan? |
|------------------------|---------------------------------------------|------------------------|
| Command Scan Start     | First command in scan range                 | Yes                   |
| Command Scan End       | Last command in scan range                  | Yes                   |
| Command Scan Pause     | Delay (ms) between commands                 | Yes                   |
| Hamulight Brightness   | Set brightness (0-100%)                     | No (direct control)   |

---

### Sensors

| Name                        | Description                             |
|-----------------------------|-----------------------------------------|
| Hamulight Last Scanned Command | Publishes last command sent during scan |

---

## Home Assistant Integration

All defined buttons, numbers, and sensors will appear as entities in Home Assistant.  
You can control your Hamulight lights via the UI, automations, or scripts.

---

## License

This project is licensed under the MIT License.  
See [LICENSE](LICENSE) for details.

---

## Acknowledgements

- [Hamulight](https://www.hamulight.nl/) for their products and inspiration
- ESPHome community for the custom component framework

---

**Questions, issues, or contributions?**  
Open an [issue](https://github.com/esphome/esphome/issues) or a [pull request](https://github.com/esphome/esphome/pulls)!

---
### Pairing process
Disconnect and reconnect LED driver from mains and send the "max brightness" command within the first 10 seconds.
