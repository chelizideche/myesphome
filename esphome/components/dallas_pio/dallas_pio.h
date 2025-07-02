#pragma once

#include "esphome/core/component.h"
#include "esphome/components/one_wire/one_wire.h"
#include "dallas_pio_constants.h"

namespace esphome {
namespace dallas_pio {

class DallasPio : public Component, public one_wire::OneWireDevice {
 public:
  DallasPio();
  std::string id_;
  optional<std::string> one_wire_id_;

  void set_id(const std::string &id);
  void set_name(const std::string &name);
  void set_address(uint64_t address);
  void set_reference(const std::string &reference);
  void set_crc_enabled(bool enabled);
  void set_one_wire_id(const std::string &one_wire_id);
  bool check_address();

  void setup() override;
  void dump_config() override;
  void initialize_reference() {
    this->family_code_ = static_cast<uint8_t>(this->address_ & 0xFF);
    switch (this->family_code_) {
      case DALLAS_MODEL_DS2413:
        this->reference_from_address_ = "DS2413";
        break;
      case DALLAS_MODEL_DS2406:
        this->reference_from_address_ = "DS2406";
        break;
      case DALLAS_MODEL_DS2408:
        this->reference_from_address_ = "DS2408";
        break;
      default:
        this->reference_from_address_ = "Unknown";
        break;
    }
    if (this->reference_.empty()) {
      if (this->reference_from_address_ == "Unknown") {
        this->reference_ = "DS2413";
      } else {
        this->reference_ = this->reference_from_address_;
      }
    }

    if (this->reference_ == "DS2413") {
      pio_output_register_ = 0xFF;
    }
  }

  bool read_state(uint8_t &state, uint8_t pin);
  bool write_state(bool state, uint8_t pin, bool pin_inverted);

  const std::string &get_name() const { return this->name_; }
  const std::string &get_id() const { return this->id_; }
  uint64_t get_address() const { return this->address_; }
  const std::string &get_reference() const { return this->reference_; }
  uint8_t get_family_code() const { return this->family_code_; }

 protected:
  std::string name_;
  std::string reference_;
  uint64_t address_;
  std::string reference_from_address_;
  uint8_t family_code_;
  bool is_setup_done_ = false;
  bool ds2413_get_state_(uint8_t &state);
  void ds2413_write_state_(bool state);
  bool ds2406_get_state_(uint8_t &state, bool use_crc);
  void ds2406_write_state_(bool state, bool use_crc);
  bool ds2408_read_device_(uint8_t &pio_logic_state, bool use_crc);
  bool ds2408_get_state_(uint8_t &state, bool use_crc);
  void ds2408_write_state_(bool state, bool use_crc);
  uint16_t crc_;
  bool crc_enabled_ = false;
  bool ds2406_verify_crc_(uint8_t control1, uint8_t control2, uint8_t channel_info_byte, uint8_t io_state,
                          uint16_t received_crc);
  void crc_reset_();
  void crc_shift_byte_(uint8_t byte);
  uint16_t crc_read_() const;
  // uint8_t calculate_crc_(std::initializer_list<uint8_t> data);
  uint8_t pin_;
  bool pin_inverted_;
  uint8_t pio_output_register_;
  uint16_t calculate_crc16_(const uint8_t *data, size_t length);
  uint8_t ds2408_write_current_state_ = 0xFF;
};

}  // namespace dallas_pio
}  // namespace esphome
