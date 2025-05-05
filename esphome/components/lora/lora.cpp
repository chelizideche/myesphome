#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "lora.h"

namespace esphome {
namespace lora {

static const char *const TAG = "lora";

void LoRa::dump_config() {
  ESP_LOGCONFIG(TAG, "LoRa:");
  ESP_LOGCONFIG(TAG, "  Platform: %s", this->platform_name_);
  // ESP_LOGCONFIG(TAG, "  Radio: %s", this->parent_->get_name().c_str());
  // ESP_LOGCONFIG(TAG, "  Encrypted: %s", YESNO(this->is_encrypted_()));
  // ESP_LOGCONFIG(TAG, "  Ping-pong: %s", YESNO(this->ping_pong_enable_));
  // Component::dump_config();
}

void LoRa::packet_received(const std::vector<uint8_t> &packet, float rssi, float snr) {
  ESP_LOGD(TAG, "packet %s", format_hex(packet).c_str());
  ESP_LOGD(TAG, "rssi %.2f", rssi);
  ESP_LOGD(TAG, "snr %.2f", snr);
  this->call_listeners_(packet, rssi, snr);
}

void LoRa::call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, rssi, snr);
  }
  this->packet_trigger_->trigger(packet, rssi, snr);
}

}  // namespace lora
}  // namespace esphome