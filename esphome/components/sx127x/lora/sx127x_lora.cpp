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

}  // namespace sx127x
}  // namespace esphome
