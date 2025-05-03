#include "landis.h"

#include <string>

namespace esphome {
namespace landis {

static const char *TAG = "UH50";

static int UpdateCounter = 0;
void LandisSensor::update() {
  // Wait a bit till we have logs
  if (UpdateCounter++ == 0)
    return;

  buffer_string.clear();

  if (state == State::Idle) {
    SendRequest();
    state = State::RecieveInit;
  } else {
    ESP_LOGE("custom", "Update called but we are not ready yet. something is wrong!");
  }
}

void LandisSensor::loop() {
  if (!(state == State::ReceiveValues || state == State::RecieveInit))
    return;

  auto line = ReadLine();
  if (line.empty())
    return;

  if (state == State::RecieveInit) {
    ESP_LOGD(TAG, "initialization message %s", line.c_str());
    this->parent_->set_baud_rate(2400);
    this->parent_->load_settings(false);
    state = State::ReceiveValues;
  } else if (state == State::ReceiveValues) {
    ESP_LOGD(TAG, "Values: %s", line.c_str());
    if (ReadOnlyFirstLine) {
      ParseFirstLine(line);
      state = State::Idle;
    } else if (buffer_string.find('!'))
      state = State::Idle;
  }
}

void LandisSensor ::dump_config() {
  ESP_LOGCONFIG(TAG, "LandisGur UH50");
  LOG_SENSOR(TAG, "Energy KWh", this->kwh_sensor_);
  LOG_SENSOR(TAG, "Water m³", this->kwh_sensor_);
}

void LandisSensor::SendRequest() {
  this->parent_->set_baud_rate(300);
  this->parent_->load_settings(false);

  for (int i = 0; i <= 40; i++)
    this->write(0x00);
  this->write_str("\x2F\x3F\x21\x0D\x0A");

  ESP_LOGD(TAG, "init message sent");
}

std::string LandisSensor::ReadLine() {
  while (available()) {
    auto c = read();
    buffer_string += c;
    if (c == '\n') {
      auto r = buffer_string;
      buffer_string.clear();
      return r;
    }
  }
  return "";
}

std::string LandisSensor::ParseDelimiter(std::string stringToParse, std::string first, std::string second) {
  auto pos1 = stringToParse.find(first);
  auto pos2 = stringToParse.find(second);
  if (pos1 == -1 || pos2 == -1) {
    ESP_LOGE(TAG, "failed to parse %s", stringToParse.c_str());
    return "";
  }

  return stringToParse.substr(pos1 + first.length(), pos2 - pos1 - first.length());
}

void LandisSensor::ParseFirstLine(std::string line) {
  std::string Energy = ParseDelimiter(line, "6.8(", "*MWh");
  std::string Water = ParseDelimiter(line, "6.26(", "*m3");

  if (kwh_sensor_ != nullptr)
    kwh_sensor_->publish_state(strtof(Energy.c_str(), nullptr));
  if (volume_sensor_ != nullptr)
    volume_sensor_->publish_state(strtof(Water.c_str(), nullptr));
}

}  // namespace landis
}  // namespace esphome
