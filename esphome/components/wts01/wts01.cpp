#include "wts01.h"
#include "esphome/core/log.h"
#include <cmath>  // Pour std::abs

namespace esphome {
namespace wts01 {

void WTS01Sensor::setup() {
  // Empty setup - initialization is handled in the loop function
}

void WTS01Sensor::update() {
  // Publish the latest temperature value at the configured update interval
  if (this->temperature_sensor_ != nullptr && !std::isnan(this->current_temperature_)) {
    // Only log when publishing if the value has changed significantly since last publish
    if (std::isnan(this->last_published_temperature_) ||
        std::abs(this->current_temperature_ - this->last_published_temperature_) >= 0.1f) {
      ESP_LOGD(TAG, "Temperature: %.2f°C", this->current_temperature_);
    }

    this->last_published_temperature_ = this->current_temperature_;
    this->temperature_sensor_->publish_state(this->current_temperature_);
  }
}

void WTS01Sensor::loop() {
  if (this->uart_ == nullptr) {
    return;
  }

  while (this->uart_->available()) {
    uint8_t c;
    if (this->uart_->read_byte(&c)) {
      this->handle_char_(c);
    }
  }
}

void WTS01Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "WTS01 Sensor:");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

void WTS01Sensor::handle_char_(uint8_t c) {
  // Reset buffer if we're at the start of a new packet
  if (c == HEADER_1 && buffer_pos_ == 0) {
    buffer_[buffer_pos_++] = c;
    return;
  }

  // Check header sequence
  if (buffer_pos_ == 1 && c != HEADER_2) {
    buffer_pos_ = 0;
    return;
  }

  if (buffer_pos_ == 2 && c != HEADER_3) {
    buffer_pos_ = 0;
    return;
  }

  if (buffer_pos_ == 3 && c != HEADER_4) {
    buffer_pos_ = 0;
    return;
  }

  // Add byte to buffer
  buffer_[buffer_pos_++] = c;

  // Process complete packet
  if (buffer_pos_ == PACKET_SIZE) {
    process_packet_();
    buffer_pos_ = 0;
  }
}

void WTS01Sensor::process_packet_() {
  // Based on Tasmota implementation
  // Format: 55 01 01 04 01 11 16 12 95
  // header            T  Td Ck  - T = Temperature, Td = Temperature decimal, Ck = Checksum

  // Check if packet is valid (we could add checksum verification here if needed)
  if (buffer_[0] == HEADER_1 && buffer_[1] == HEADER_2 && buffer_[2] == HEADER_3 && buffer_[3] == HEADER_4) {
    // Extract temperature value
    uint8_t temp = buffer_[6];
    int sign = 1;

    // Handle negative temperatures
    if (temp > 127) {
      temp -= 128;
      sign = -1;
    }

    // Calculate temperature (temp + decimal/100)
    float temperature = sign * (static_cast<float>(temp) + (static_cast<float>(buffer_[7]) / 100.0f));

    // Store the temperature value but don't publish yet - wait for update() to be called
    this->current_temperature_ = temperature;
  }
}

}  // namespace wts01
}  // namespace esphome
