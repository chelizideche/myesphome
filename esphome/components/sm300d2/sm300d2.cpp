//Duker4444 - Notes
//This has been a complete rewrite of how the data is parsed. 
// This now recieves the data strings and parses out both the original 17 octet and the newer 19 octet data strings. 
// It places these into a recieve buffer and then pulles the data out.
// It also keeps partial datastrings for assembly when the rest is recived. 
// Once it see a timestamp update, it will clear the buffer to ensure no stale date is kept.

#include "sm300d2.h"
#include "esphome/core/log.h"
#include <vector> // For std::vector

namespace esphome {
namespace sm300d2 {

static const char *const TAG = "sm300d2";
static const uint8_t SM300D2_LEGACY_RESPONSE_LENGTH = 17;
static const uint8_t SM300D2_NEW_RESPONSE_LENGTH = 19;
static const uint32_t MAX_BUFFER_SIZE = 256; // Matches UART RX buffer size
static const uint16_t MAX_REASONABLE_PM = 1000; // Max PM2.5/PM10 (µg/m³)

std::vector<uint8_t> buffer_; // Buffer for incoming data

void SM300D2Sensor::update() {
  // Read all available data into the buffer
  while (this->available()) {
    uint8_t byte = this->read();
    buffer_.push_back(byte);
    if (buffer_.size() > MAX_BUFFER_SIZE) {
      buffer_.erase(buffer_.begin(), buffer_.begin() + (buffer_.size() - MAX_BUFFER_SIZE));
    }
  }

  // Log buffer size for debugging
  ESP_LOGD(TAG, "Buffer size: %d bytes", buffer_.size());

  // Search for preambles and process packets
  size_t pos = 0;
  while (pos < buffer_.size()) {
    if (buffer_[pos] == 0x3C || buffer_[pos] == 0x01) {
      uint8_t preamble = buffer_[pos];
      uint8_t packet_length = (preamble == 0x3C) ? SM300D2_LEGACY_RESPONSE_LENGTH : SM300D2_NEW_RESPONSE_LENGTH;
      if (pos + packet_length <= buffer_.size()) {
        // Extract packet
        std::vector<uint8_t> packet(buffer_.begin() + pos, buffer_.begin() + pos + packet_length);
        // Process packet
        process_packet(packet, preamble == 0x01);
        // Remove processed bytes
        buffer_.erase(buffer_.begin(), buffer_.begin() + pos + packet_length);
        pos = 0; // Reset position after erasure
      } else {
        // Partial packet, wait for more data
        break;
      }
    } else {
      // Skip invalid byte
      pos++;
    }
  }

  // Remove processed or invalid bytes, keep partial packet
  if (pos > 0) {
    buffer_.erase(buffer_.begin(), buffer_.begin() + pos);
  }
}

void SM300D2Sensor::process_packet(const std::vector<uint8_t>& packet, bool is_new_revision) {
  // Log raw packet
  std::string hex;
  for (uint8_t byte : packet) {
    char buf[4];
    sprintf(buf, "%02X ", byte);
    hex += buf;
  }
  ESP_LOGD(TAG, "Processing packet (%s): %s", is_new_revision ? "new revision" : "legacy", hex.c_str());

  // Validate preamble
  if (is_new_revision && packet[0] != 0x01) {
    ESP_LOGW(TAG, "Invalid preamble for new revision: 0x%02X", packet[0]);
    return;
  }
  if (!is_new_revision && (packet[0] != 0x3C || packet[1] != 0x02)) {
    ESP_LOGW(TAG, "Invalid preamble for legacy: 0x%02X 0x%02X", packet[0], packet[1]);
    return;
  }

  // Validate checksum (legacy only)
  if (!is_new_revision) {
    uint16_t calculated_checksum = sm300d2_checksum_(packet.data(), packet.size());
    if (calculated_checksum != packet.back() &&
        calculated_checksum - 0x80 != packet.back() &&
        calculated_checksum + 0x80 != packet.back()) {
      ESP_LOGW(TAG, "Checksum mismatch: 0x%02X != 0x%02X", packet.back(), calculated_checksum);
      return;
    }
  } else {
    // Log checksum info for new revision
    uint8_t expected_checksum = packet[packet.size() - 2];
    uint8_t calculated_sum = 0;
    for (size_t i = 0; i < packet.size() - 2; i++) calculated_sum += packet[i];
    ESP_LOGD(TAG, "New revision checksum (sum B0–B16): 0x%02X, expected: 0x%02X", calculated_sum, expected_checksum);
    // Test alternative checksum (B0–B14)
    uint8_t sum_b0_b14 = 0;
    for (size_t i = 0; i < 15; i++) sum_b0_b14 += packet[i];
    ESP_LOGD(TAG, "Test checksum (B0–B14): 0x%02X, expected: 0x%02X", sum_b0_b14, expected_checksum);
  }

  // Parse sensor data
  uint8_t offset = is_new_revision ? 3 : 2;
  const uint16_t addr = is_new_revision ? packet[0] : 0;
  const uint16_t function = is_new_revision ? (packet[1] * 256 + packet[2]) : 0;
  const uint16_t co2 = (packet[offset] * 256) + packet[offset + 1];
  const uint16_t formaldehyde = (packet[offset + 2] * 256) + packet[offset + 3];
  const uint16_t tvoc = (packet[offset + 4] * 256) + packet[offset + 5];
  const uint16_t pm_2_5 = (packet[offset + 6] * 256) + packet[offset + 7];
  const uint16_t pm_10_0 = (packet[offset + 8] * 256) + packet[offset + 9];
  const float temperature = ((packet[offset + 10] + (packet[offset + 11] * 0.1f)) > 128)
                              ? (((packet[offset + 10] + (packet[offset + 11] * 0.1f)) - 128) * -1)
                              : packet[offset + 10] + (packet[offset + 11] * 0.1f);
  const float humidity = packet[offset + 12] + (packet[offset + 13] * 0.1f);

  // Validate sensor values
  bool valid_data = true;
  if (pm_2_5 > MAX_REASONABLE_PM || pm_10_0 > MAX_REASONABLE_PM) {
    ESP_LOGW(TAG, "Unrealistic PM values: PM2.5=%u, PM10=%u µg/m³", pm_2_5, pm_10_0);
    valid_data = false;
  }
  if (temperature < -40.0f || temperature > 80.0f) {
    ESP_LOGW(TAG, "Unrealistic temperature: %.2f °C", temperature);
    valid_data = false;
  }
  if (humidity < 0.0f || humidity > 100.0f) {
    ESP_LOGW(TAG, "Unrealistic humidity: %.2f %%", humidity);
    valid_data = false;
  }

  if (!valid_data) {
    status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Publish sensor data
  if (is_new_revision) {
    ESP_LOGD(TAG, "Received Address: %u", addr);
    if (this->addr_sensor_ != nullptr) this->addr_sensor_->publish_state(addr);
    ESP_LOGD(TAG, "Received Function Type: %u", function);
    if (this->function_sensor_ != nullptr) this->function_sensor_->publish_state(function);
  }
  ESP_LOGD(TAG, "Received CO₂: %u ppm", co2);
  if (this->co2_sensor_ != nullptr) this->co2_sensor_->publish_state(co2);
  ESP_LOGD(TAG, "Received Formaldehyde: %u µg/m³", formaldehyde);
  if (this->formaldehyde_sensor_ != nullptr) this->formaldehyde_sensor_->publish_state(formaldehyde);
  ESP_LOGD(TAG, "Received TVOC: %u µg/m³", tvoc);
  if (this->tvoc_sensor_ != nullptr) this->tvoc_sensor_->publish_state(tvoc);
  ESP_LOGD(TAG, "Received PM2.5: %u µg/m³", pm_2_5);
  if (this->pm_2_5_sensor_ != nullptr) this->pm_2_5_sensor_->publish_state(pm_2_5);
  ESP_LOGD(TAG, "Received PM10: %u µg/m³", pm_10_0);
  if (this->pm_10_0_sensor_ != nullptr) this->pm_10_0_sensor_->publish_state(pm_10_0);
  ESP_LOGD(TAG, "Received Temperature: %.2f °C", temperature);
  if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state(temperature);
  ESP_LOGD(TAG, "Received Humidity: %.2f percent", humidity);
  if (this->humidity_sensor_ != nullptr) this->humidity_sensor_->publish_state(humidity);
}

uint16_t SM300D2Sensor::sm300d2_checksum_(const uint8_t *ptr, uint8_t length) {
  uint8_t sum = 0;
  for (int i = 0; i < length - 1; i++) {
    sum += ptr[i];
  }
  return sum;
}

void SM300D2Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SM300D2:");
  LOG_SENSOR("  ", "Address", this->addr_sensor_);
  LOG_SENSOR("  ", "Function", this->function_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  this->check_uart_settings(9600);
}

} // namespace sm300d2
} // namespace esphome
