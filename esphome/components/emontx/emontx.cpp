#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace emontx {

static const char *const TAG = "emontx";

/**
 * @brief Initializes the EmonTx by setting the initial state to OFF.
 */
void EmonTx::setup() { ESP_LOGCONFIG(TAG, "Setting up EmonTx Component..."); }

/**
 * @brief Updates the EmonTx state. Resets the buffer index and transitions the state from OFF to ON.
 */
void EmonTx::update() {
  ESP_LOGD(TAG, "Updating EmonTx state...");
  buffer_.clear();  // Clear the buffer for new data
  ESP_LOGD(TAG, "EmonTx is now ready to receive data.");
}

/**
 * @brief Implements the main state machine for processing incoming data.
 *
 * @details The state machine transitions through the following states:
 * - OFF: Does nothing.
 * - ON: Reads characters until the start frame (0x2) is found.
 * - START_FRAME_RECEIVED: Reads characters until the end frame (0x3) is found.
 * - END_FRAME_RECEIVED: Processes the buffer to extract groups, validate CRC, and publish values.
 */
void EmonTx::loop() {
  while (available()) {
    char c = read();

    if (c == '\n' || c == '\r') {
      if (!buffer_.empty()) {
        parse_json_(buffer_);
        buffer_.clear();
      }
    } else {
      buffer_ += c;
      // Prevent buffer overflow
      if (buffer_.length() > 1024) {
        ESP_LOGW(TAG, "Buffer overflow, clearing buffer");
        buffer_.clear();
      }
    }
  }
}

void EmonTx::parse_json_(const std::string &data) {
  ESP_LOGV(TAG, "Parsing JSON: %s", data.c_str());

  bool success = json::parse_json(data, [this](JsonObject root) {
    // Update all registered sensors
    for (auto &sensor_pair : this->sensors_) {
      const std::string &tag = sensor_pair.first;
      sensor::Sensor *sensor = sensor_pair.second;

      if (root.containsKey(tag)) {
        float value = root[tag];
        ESP_LOGD(TAG, "Updating sensor '%s' with value: %.2f", tag.c_str(), value);
        sensor->publish_state(value);
      }
    }
    return true;  // Parsing was handled successfully
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON");
  }
}

void EmonTx::register_sensor(const std::string &tag, sensor::Sensor *sensor) {
  ESP_LOGD(TAG, "Registering sensor for tag: %s", tag.c_str());
  sensors_[tag] = sensor;
}

/**
 * @brief Dumps the EmonTx configuration to the log.
 *
 * @note Logs the UART settings and other configuration details.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx Component:");
  ESP_LOGCONFIG(TAG, "  Registered sensors: %d", sensors_.size());
  for (auto &sensor_pair : sensors_) {
    ESP_LOGCONFIG(TAG, "    Tag: %s", sensor_pair.first.c_str());
  }
}

/**
 * @brief Registers a listener to receive updates for specific tags.
 *
 * @param listener Pointer to the listener to register.
 */
void EmonTx::register_emontx_listener(EmonTxListener *listener) { emontx_listeners_.push_back(listener); }

}  // namespace emontx
}  // namespace esphome
