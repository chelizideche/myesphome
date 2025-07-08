esphome:
  name: vl53l4cd_node
#  includes:
#    - vl53l4cd/vl53l4cd_sensor.h

esp32:
  board: esp32-c3-devkitm-1
  framework: 
    type: arduino

logger:
  level: DEBUG

i2c:
  sda: 2
  scl: 3
  scan: true

sensor:
  - platform: vl53l4cd
    name: "VL53L4CD Distance"
    address: 0x29