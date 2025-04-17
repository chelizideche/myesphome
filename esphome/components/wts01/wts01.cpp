#include "wts01.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace wts01 {

static const char *const TAG = "wts01";
static const uint8_t PACKET_SIZE = 9;
static const uint8_t HEADER_1 = 0x55;
static const uint8_t HEADER_2 = 0x01;
static const uint8_t HEADER_3 = 0x01;
static const uint8_t HEADER_4 = 0x04;

void WTS01Sensor::loop() {
  // Process characters received from the sensor
  while (available()) {
    uint8_t c;
    if (read_byte(&c)) {
      this->handle_char_(c);
    }
  }
}

void WTS01Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "WTS01 Temperature Sensor:");
  LOG_SENSOR("  ", "Temperature", this);
  this->check_uart_settings(9600);
}

void WTS01Sensor::handle_char_(uint8_t c) {
  // Reset buffer if we're at the start of a new packet
  if (c == HEADER_1 && this->buffer_pos_ == 0) {
    this->buffer_[this->buffer_pos_++] = c;
    return;
  }

  // Check header sequence
  if (this->buffer_pos_ == 1 && c != HEADER_2) {
    this->buffer_pos_ = 0;
    return;
  }

  if (this->buffer_pos_ == 2 && c != HEADER_3) {
    this->buffer_pos_ = 0;
    return;
  }

  if (this->buffer_pos_ == 3 && c != HEADER_4) {
    this->buffer_pos_ = 0;
    return;
  }

  // Add byte to buffer
  this->buffer_[this->buffer_pos_++] = c;

  // Process complete packet
  if (this->buffer_pos_ == PACKET_SIZE) {
    this->process_packet_();
    this->buffer_pos_ = 0;
  }
}

void WTS01Sensor::process_packet_() {
  // Based on Tasmota implementation
  // Format: 55 01 01 04 01 11 16 12 95
  // header            T  Td Ck  - T = Temperature, Td = Temperature decimal, Ck = Checksum

  // Check if packet is valid (we could add checksum verification here if needed)
  if (this->buffer_[0] == HEADER_1 && this->buffer_[1] == HEADER_2 && this->buffer_[2] == HEADER_3 &&
      this->buffer_[3] == HEADER_4) {
    // Extract temperature value
    uint8_t temp = this->buffer_[6];
    int sign = 1;

    // Handle negative temperatures
    if (temp > 127) {
      temp -= 128;
      sign = -1;
    }

    // Calculate temperature (temp + decimal/100)
    float temperature = sign * (static_cast<float>(temp) + (static_cast<float>(this->buffer_[7]) / 100.0f));

    // Store temperature
    this->current_temperature_ = temperature;
    ESP_LOGV(TAG, "Temperature: %.2f°C", temperature);

    this->publish_state(temperature);
  }
}

}  // namespace wts01
}  // namespace esphome
