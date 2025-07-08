#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "lora_transport.h"

namespace esphome {
namespace lora {

static const char *const TAG = "lora_transport";

void LoRaTransport::update() {
  PacketTransport::update();
  this->updated_ = true;
  this->resend_data_ = true;
}

void LoRaTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->send_packet(buf); }

void LoRaTransport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  ESP_LOGV(TAG, "Reveived packet of size %u with RSSI: %.1f, SNR: %.1f", packet.size(), rssi, snr);
  this->process_(packet);
}

}  // namespace lora
}  // namespace esphome
