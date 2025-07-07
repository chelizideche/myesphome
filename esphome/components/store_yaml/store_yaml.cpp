#include "store_yaml.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace store_yaml {

static const char *const TAG = "store_yaml";

void StoreYamlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "YAML:");
  ESP_LOGCONFIG(TAG, "  Compressed size: %zu", ESPHOME_YAML_SIZE);
#ifndef USE_RP2040
  const char *url = (this->web_server_ != nullptr) ? this->web_server_url_.c_str() : "not configured!";
  ESP_LOGCONFIG(TAG, "  Web server url: %s", url);
#endif
  if (this->show_in_dump_config_) {
    RowDecompressor dec(ESPHOME_YAML, ESPHOME_YAML_SIZE);
    std::string row;
    while (dec.get_row(row)) {
      ESP_LOGCONFIG(TAG, "%s", row.c_str());
    }
  }
}

void StoreYamlComponent::setup() {
#ifndef USE_RP2040
  if (this->web_server_ != nullptr) {
    this->web_server_->init();
    this->web_server_->add_handler(this);
    ESP_LOGD(TAG, "Web server configured to serve at: %s", this->web_server_url_.c_str());
  }
#endif
}

void StoreYamlComponent::loop() {
  if (this->dec_) {
    std::string row;
    if (this->dec_->get_row(row)) {
      ESP_LOGI(TAG, "%s", row.c_str());
    } else {
      this->dec_ = nullptr;
    }
  }
}

void StoreYamlComponent::log() {
  ESP_LOGI(TAG, "Decompressed YAML:");
  this->dec_ = make_unique<RowDecompressor>(ESPHOME_YAML, ESPHOME_YAML_SIZE);
}

#ifndef USE_RP2040

void StoreYamlComponent::set_web_server(web_server_base::WebServerBase *web_server, const std::string &url) {
  this->web_server_ = web_server;
  this->web_server_url_ = url;
}

bool StoreYamlComponent::canHandle(AsyncWebServerRequest *request) {
  return request->method() == HTTP_GET && request->url() == this->web_server_url_.c_str();
}

void StoreYamlComponent::handleRequest(AsyncWebServerRequest *request) {
#ifdef USE_ARDUINO
  auto cb = [this](uint8_t *buffer, size_t max_len, size_t index) -> size_t {
    uint8_t *ptr = buffer;
    // 5KB+ config file with a single character repeating will result in a 100 byte long word, not likely
    while (max_len > 100) {
      std::string s;
      if (!this->web_dec_) {
        this->web_dec_ = make_unique<Decompressor>(ESPHOME_YAML, ESPHOME_YAML_SIZE);
        s = this->web_dec_->get_first();
      } else {
        s = this->web_dec_->get_next();
      }
      size_t len = std::min(max_len, s.size());
      memcpy(ptr, s.c_str(), len);
      ptr += len;
      max_len -= len;
      if (this->web_dec_->is_eof()) {
        this->web_dec_ = nullptr;
        break;
      }
    }
    return ptr - buffer;
  };
  this->web_dec_ = nullptr;
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain;charset=UTF-8", cb);
#else
  AsyncResponseStream *response = request->beginResponseStream("text/plain;charset=UTF-8");
  auto dec = make_unique<Decompressor>(ESPHOME_YAML, ESPHOME_YAML_SIZE);
  response->print(dec->get_first().c_str());
  while (!dec->is_eof()) {
    response->print(dec->get_next().c_str());
  }
  dec = nullptr;
#endif
  request->send(response);
}

#endif

}  // namespace store_yaml
}  // namespace esphome
