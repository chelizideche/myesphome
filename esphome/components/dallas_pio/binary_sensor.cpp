#include "binary_sensor.h"

namespace esphome {
namespace dallas_pio {

void DallasPioBinarySensor::setup() {
  if (this->dallas_pio_ == nullptr) {
    ESP_LOGE(TAG, "DallasPioBinarySensor setup failed - DallasPio not set for this binary sensor.");
    return;
  }
  ESP_LOGI(TAG, "DallasPioBinarySensor setup - DallasPio set for this binary sensor.");

  this->address_ = this->dallas_pio_->get_address();
  this->reference_ = this->dallas_pio_->get_reference();
  this->family_code_ = this->dallas_pio_->get_family_code();
  //  this->bus_ = this->dallas_pio_->get_bus();
}

void DallasPioBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Dallas PIO Binary Sensor:");
  ESP_LOGCONFIG(TAG, "  dallas_pio_id: %s", this->dallas_pio_->get_id().c_str());

  if (this->reference_ == "DS2408") {
    ESP_LOGCONFIG(TAG, "  pin: P%c", '0' - 1 + this->pin_);  // P0, P1, ...
  } else {
    ESP_LOGCONFIG(TAG, "  pin: PIO%c", 'A' - 1 + this->pin_);  // PIOA, PIOB, ...
  }
  ESP_LOGCONFIG(TAG, "  pin inverted: %s", this->pin_inverted_ ? "true" : "false");
  /*
  if (this->address_ == 0) {
    ESP_LOGCONFIG(TAG, "  address: invalid (null)");
  } else {
    ESP_LOGCONFIG(TAG, "  address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                 (uint8_t) (this->address_ >> 56),
                 (uint8_t) (this->address_ >> 48),
                 (uint8_t) (this->address_ >> 40),
                 (uint8_t) (this->address_ >> 32),
                 (uint8_t) (this->address_ >> 24),
                 (uint8_t) (this->address_ >> 16),
                 (uint8_t) (this->address_ >> 8),
                 (uint8_t) (this->address_));
    ESP_LOGCONFIG(TAG, "  Family code: 0x%02X", this->family_code_);
    ESP_LOGCONFIG(TAG, "  Reference: %s", this->reference_.c_str());
  }
  */
  ESP_LOGCONFIG(TAG, "  Name: %s", this->EntityBase::name_.c_str());

  //  this->component_->log_one_wire_device();
  LOG_UPDATE_INTERVAL(this);
}

void DallasPioBinarySensor::update() {
  // Ref Analog Devices DS2413.pdf p6 of 19
  // | b7      b6      b5      b4     |  b3          | b2       |  b1          | b0       |
  // |    Complement of b3 to b0      | PIOB Output  | PIOB Pin | PIOA Output  | PIOA Pin |
  // |                                | Latch State  | State    | Latch State  | State    |
  //  ESP_LOGCONFIG(TAG, "Dallas PIO Binary Sensor Update !");
  uint8_t state = 0;
  if (this->dallas_pio_->check_address() == 0) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  if (!this->dallas_pio_->read_state(state, this->pin_)) {
    return;
  }

  if (this->pin_inverted_) {
    state = !state;
  }
  this->publish_state(state);
}

}  // namespace dallas_pio
}  // namespace esphome
