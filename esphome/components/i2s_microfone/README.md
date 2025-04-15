Example:
```yaml
substitutions:
  I2S_MIC_WS_pin: GPIO6
  I2S_MIC_SCK_pin: GPIO7
  I2S_MIC_SD_pin: GPIO21

esphome:
  name: i2s_microfone_sample
  friendly_name: i2s microfone sample

esp32:
  board: esp32dev
  framework:
    type: arduino

logger:

api:
  encryption:
    key: "< USE YOURS >"

ota:
  - platform: esphome
    password: "< USE YOURS >"

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  ap:
    ssid: "Test-Dfrobot-Air-Quality-Sensor"
    password: "< USE YOURS >"

captive_portal:

external_components:
  - source:
      type: local
      path: components

number:
  - platform: template
    name: "Mic Calibration"
    id: mic_calibration_offset
    entity_category: config
    optimistic: true
    mode: box
    min_value: -100.0
    max_value: 100.0
    step: 0.1
    initial_value: 0.0
    restore_value: true    

sensor:
  - platform: i2s_microfone
    name: "Sound Level"
    id: mic_level
    update_interval: 250ms
    unit_of_measurement: "dB"
    bck_pin: ${I2S_MIC_SCK_pin}
    ws_pin: ${I2S_MIC_WS_pin}
    data_in_pin: ${I2S_MIC_SD_pin}
    accuracy_decimals: 1
    calibration_offset: 0.0
    calibration_number: mic_calibration_offset
    icon: "mdi:microphone"
```
