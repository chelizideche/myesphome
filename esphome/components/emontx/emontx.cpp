#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

#ifdef USE_ESP32
#include <HTTPClient.h>
#elif defined(USE_ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#endif

namespace esphome {
namespace emontx {

static const char *const TAG = "emontx";

/**
 * @brief Initializes the EmonTx.
 */
void EmonTx::setup() {
  ESP_LOGCONFIG(TAG, "Setting up EmonTx component");

  // Log sensors at setup time
  ESP_LOGCONFIG(TAG, "Currently registered sensors: %u", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor '%s' registered", sensor_pair.first.c_str());
  }

  // Log EmonCMS configuration
  if (has_emoncms_config_) {
    ESP_LOGCONFIG(TAG, "EmonCMS forwarding enabled:");
    ESP_LOGCONFIG(TAG, "  Server: %s", this->emoncms_server_.c_str());
    ESP_LOGCONFIG(TAG, "  Node: %s", this->emoncms_node_.c_str());
    ESP_LOGCONFIG(TAG, "  API Key: %s", this->emoncms_apikey_.substr(0, 5).c_str());
  }
}

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

        // Forward to EmonCMS if configured
        if (has_emoncms_config_ && network::is_connected()) {
          send_to_emoncms_(buffer_);
        }

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

    // Also update all listeners
    for (auto *listener : this->emontx_listeners_) {
      if (root.containsKey(listener->tag)) {
        const auto value = root[listener->tag].as<std::string>();
        listener->publish_val(value);
      }
    }

    return true;  // Parsing was handled successfully
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON");
  }
}

void EmonTx::send_to_emoncms_(const std::string &json_data) {
  if (!has_emoncms_config_) {
    return;
  }

  ESP_LOGV(TAG, "Sending data to EmonCMS: %s", json_data.c_str());

#if defined(USE_ESP32) || defined(USE_ESP8266)
  HTTPClient http;

  // Build the URL - format: server/input/post.json?node=nodename&apikey=apikey&json={...}
  std::string url = emoncms_server_;
  if (url.back() != '/') {
    url += '/';
  }

  url += "input/post.json?node=";
  url += emoncms_node_;
  url += "&apikey=";
  url += emoncms_apikey_;
  url += "&fulljson=";
  url += json_data;

  ESP_LOGV(TAG, "EmonCMS URL: %s", url.c_str());

#ifdef USE_ESP32
  http.begin(url.c_str());
#elif defined(USE_ESP8266)
  WiFiClient client;
  http.begin(client, url.c_str());
#endif

  int http_response_code = http.GET();

  if (http_response_code > 0) {
    ESP_LOGV(TAG, "HTTP Response code: %d", http_response_code);
    std::string payload = http.getString().c_str();
    ESP_LOGV(TAG, "Response: %s", payload.c_str());
  } else {
    ESP_LOGW(TAG, "Error sending to EmonCMS. Error code: %d", http_response_code);
  }

  http.end();
#endif
}

void EmonTx::register_sensor(const std::string &tag_name, sensor::Sensor *sensor) {
  ESP_LOGCONFIG(TAG, "Registering sensor for tag: %s", tag_name.c_str());
  this->sensors_[tag_name] = sensor;
}

/**
 * @brief Dumps the EmonTx configuration to the log.
 *
 * @note Logs the UART settings and other configuration details.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx:");
  ESP_LOGCONFIG(TAG, "  Registered sensors: %u", this->sensors_.size());

  // List all registered sensors with their tags
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor_pair.first.c_str());
  }

  // Show EmonCMS configuration
  if (has_emoncms_config_) {
    ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: ENABLED");
    ESP_LOGCONFIG(TAG, "    Server: %s", this->emoncms_server_.c_str());
    ESP_LOGCONFIG(TAG, "    Node: %s", this->emoncms_node_.c_str());
    ESP_LOGCONFIG(TAG, "    API Key: %s...", this->emoncms_apikey_.substr(0, 5).c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: DISABLED");
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
