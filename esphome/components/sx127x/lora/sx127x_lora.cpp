#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "sx127x_lora.h"

namespace esphome {
namespace sx127x {

static const char *const TAG = "sx127x_lora";

void SX127xLoRa::set_frequency(uint32_t frequency) {
  this->parent_->set_frequency(frequency);
  this->parent_->configure();
}

void SX127xLoRa::set_mode(lora::LoRaMode mode) {
  switch (mode) {
    case lora::LoRaMode::SLEEP:
      this->parent_->set_mode_sleep();
      break;
    case lora::LoRaMode::STANDBY:
      this->parent_->set_mode_standby();
      break;
    case lora::LoRaMode::RX:
      this->parent_->set_mode_rx();
      break;
    case lora::LoRaMode::TX:
      this->parent_->set_mode_tx();
      break;
  }
}

}  // namespace sx127x
}  // namespace esphome
