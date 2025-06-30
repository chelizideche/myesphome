# DFRobot Air Quality Sensor

Example:
```yaml
esphome:
  name: test-dfrobot-air-quality-sensor
  friendly_name: Test_dfrobot_air_quality_sensor

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

i2c:
  sda: GPIO4
  scl: GPIO5
  scan: true

sensor:
  - platform: dfrobot_air_quality_sensor
    id: air_quality_sensor_level_sensor
    update_interval: 60s
    address: 0x19
    pm_1_0:
      name: "PM1.0"
      id: pm1_0_sensor
      unit_of_measurement: "µg/m³"
      icon: mdi:chemical-weapon
    pm_2_5:
      name: "PM2.5"
      id: pm2_5_sensor
      unit_of_measurement: "µg/m³"
      icon: mdi:chemical-weapon
    pm_10:
      name: "PM10"
      id: pm10_sensor
      unit_of_measurement: "µg/m³"
      icon: mdi:chemical-weapon
    aqi:
      name: "AQI"
      id: aqi_sensor
      calculation_type: "CAQI"
      icon: mdi:air-filter  
    pm_0_3um:
      name: "PM0.3"
      id: pm03_sensor
      unit_of_measurement: "um"
      icon: mdi:chemical-weapon
    pm_0_5um:
      name: "PM0.5"
      id: pm05_sensor
      unit_of_measurement: "um"
      icon: mdi:chemical-weapon
    pm_5_0um:
      name: "PM5.0"
      id: pm5_0_sensor
      unit_of_measurement: "um"
      icon: mdi:chemical-weapon
    pm_10_5um:
      name: "PM10.5"
      id: pm10_5_sensor
      unit_of_measurement: "um"
      icon: mdi:chemical-weapon  
```

