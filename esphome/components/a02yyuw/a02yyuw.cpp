// Datasheet https://www.dypcn.com/uploads/A02-Datasheet.pdf

#include "a02yyuw.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace a02yyuw {

static const char *const TAG = "a02yyuw.sensor";

void A02yyuwComponent::update() {
  if (this->model_ == A02YYTW) {
    this->write_byte(0xFF);
    ESP_LOGV(TAG, "Request read out from sensor");
  }
}

void A02yyuwComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    this->read_byte(&data);
    if (this->buffer_.empty() && (data != 0xff))
      continue;
    buffer_.push_back(data);
    if (this->buffer_.size() == 4)
      this->check_buffer_();
  }
}

void A02yyuwComponent::check_buffer_() {
  uint8_t checksum = this->buffer_[0] + this->buffer_[1] + this->buffer_[2];
  if (this->buffer_[3] == checksum) {
    float distance = (this->buffer_[1] << 8) + this->buffer_[2];
    if (distance > 30) {
      ESP_LOGV(TAG, "Distance from sensor: %f mm", distance);
      this->publish_state(distance);
    } else {
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", format_hex_pretty(this->buffer_).c_str());
    }
  } else {
    ESP_LOGW(TAG, "checksum failed: %02x != %02x", checksum, this->buffer_[3]);
  }
  this->buffer_.clear();
}

void A02yyuwComponent::dump_config() {
  switch (this->model_) {
    case A02YYUW:
      ESP_LOGCONFIG(TAG, "  sensor model: a02yyuw");
      break;
    case A02YYTW:
      ESP_LOGCONFIG(TAG, "  sensor model: a02yytw");
      LOG_UPDATE_INTERVAL(this);
      break;
  }
}

}  // namespace a02yyuw
}  // namespace esphome
