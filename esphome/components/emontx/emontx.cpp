#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"

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

#ifdef USE_HTTP_REQUEST
  // Initialize HTTP client if EmonCMS is configured
  if (has_emoncms_config_) {
    ESP_LOGCONFIG(TAG, "EmonCMS forwarding enabled:");
    ESP_LOGCONFIG(TAG, "  Server: %s", this->emoncms_server_.c_str());
    ESP_LOGCONFIG(TAG, "  Node: %s", this->emoncms_node_.c_str());
    ESP_LOGCONFIG(TAG, "  API Key: %s...", this->emoncms_apikey_.substr(0, 5).c_str());
  }
#endif
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

#ifdef USE_HTTP_REQUEST
        // Forward to EmonCMS if configured
        if (has_emoncms_config_ && network::is_connected()) {
          send_to_emoncms_(buffer_);
        }
#endif

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

#ifdef USE_HTTP_REQUEST
void EmonTx::send_to_emoncms_(const std::string &json_data) {
  if (!has_emoncms_config_) {
    return;
  }

  if (!network::is_connected()) {
    ESP_LOGW(TAG, "Network not connected, skipping EmonCMS update");
    return;
  }

  ESP_LOGV(TAG, "Sending data to EmonCMS: %s", json_data.c_str());

  // Build the URL for POST
  std::string url = emoncms_server_;
  if (url.back() != '/') {
    url += '/';
  }
  url += "input/post";

  // Create body format: node=nodename&apikey=apikey&json=jsondata
  std::string body = "node=";
  body += emoncms_node_;
  body += "&apikey=";
  body += emoncms_apikey_;
  body += "&data=";
  body += json_data;

  ESP_LOGV(TAG, "EmonCMS POST data: %s", body.c_str());

  // Send POST request and capture the response container
  auto container = http_client_->post(url, body);

  // Check if request was successfully initiated
  if (container != nullptr) {
    ESP_LOGI(TAG, "HTTP POST request to EmonCMS initiated successfully");

    // Access status code from the container
    int status_code = container->status_code;

    if (status_code >= 200 && status_code < 300) {
      ESP_LOGI(TAG, "EmonCMS data sent successfully (HTTP %d)", status_code);

      // Check if there's content to read
      if (container->content_length > 0) {
        // Read response body in chunks
        const size_t buffer_size = 128;
        uint8_t buffer[buffer_size];
        std::string response;

        int bytes_read;
        while ((bytes_read = container->read(buffer, buffer_size)) > 0) {
          response.append(reinterpret_cast<char *>(buffer), bytes_read);
        }

        if (!response.empty()) {
          ESP_LOGV(TAG, "Response: %s", response.c_str());
        }
      }
    } else {
      ESP_LOGW(TAG, "Error sending data to EmonCMS (HTTP %d)", status_code);
    }

    // Always end the connection when done
    container->end();
  } else {
    ESP_LOGW(TAG, "Failed to initiate HTTP POST request to EmonCMS");
  }
}
#endif

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

#ifdef USE_HTTP_REQUEST
  // Show EmonCMS configuration
  if (has_emoncms_config_) {
    ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: ENABLED");
    ESP_LOGCONFIG(TAG, "    Server: %s", this->emoncms_server_.c_str());
    ESP_LOGCONFIG(TAG, "    Node: %s", this->emoncms_node_.c_str());
    ESP_LOGCONFIG(TAG, "    API Key: %s...", this->emoncms_apikey_.substr(0, 5).c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: DISABLED");
  }
#else
  ESP_LOGCONFIG(TAG, "  EmonCMS Forwarding: DISABLED");
#endif
}

/**
 * @brief Registers a listener to receive updates for specific tags.
 *
 * @param listener Pointer to the listener to register.
 */
void EmonTx::register_emontx_listener(EmonTxListener *listener) { emontx_listeners_.push_back(listener); }

void EmonTx::register_sensor(const std::string &tag_name, sensor::Sensor *sensor) {
  ESP_LOGCONFIG(TAG, "Registering sensor for tag: %s", tag_name.c_str());
  this->sensors_[tag_name] = sensor;
}
}  // namespace emontx
}  // namespace esphome
