#include "dallas_pio.h"

namespace esphome {
namespace dallas_pio {

DallasPio::DallasPio()
    : id_(""),
      one_wire_id_(),
      name_(""),
      reference_(""),
      address_(0),
      reference_from_address_(""),
      family_code_(0),
      is_setup_done_(false) {}

void DallasPio::set_id(const std::string &id) { this->id_ = id; }

void DallasPio::set_name(const std::string &name) { this->name_ = name; }

void DallasPio::set_address(uint64_t address) {
  this->address_ = address;
  OneWireDevice::set_address(address);
}

void DallasPio::set_reference(const std::string &reference) { this->reference_ = reference; }

void DallasPio::set_crc_enabled(bool enabled) { this->crc_enabled_ = enabled; }

void DallasPio::set_one_wire_id(const std::string &one_wire_id) { this->one_wire_id_ = one_wire_id; }

bool DallasPio::check_address() { return this->check_address_(); }

void DallasPio::setup() {
  if (this->is_setup_done_)
    return;
  // ESP_LOGD("dallas_pio", "Setting up DallasPio: %s", this->name_.c_str());
  this->initialize_reference();
  this->is_setup_done_ = true;
}

void DallasPio::dump_config() {
  ESP_LOGCONFIG(TAG, "Dallas PIO :");
  ESP_LOGCONFIG(TAG, "  Id: %s", this->id_.c_str());
  ESP_LOGCONFIG(TAG, "  Reference: %s", this->reference_.c_str());
  if (this->address_ == 0) {
    ESP_LOGCONFIG(TAG, "  address: invalid (null)");
  } else {
    ESP_LOGCONFIG(TAG, "  address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t) (this->address_ >> 56),
                  (uint8_t) (this->address_ >> 48), (uint8_t) (this->address_ >> 40), (uint8_t) (this->address_ >> 32),
                  (uint8_t) (this->address_ >> 24), (uint8_t) (this->address_ >> 16), (uint8_t) (this->address_ >> 8),
                  (uint8_t) (this->address_));
    ESP_LOGCONFIG(TAG, "  Family code: 0x%02X", this->family_code_);
    if (reference_ != reference_from_address_) {
      ESP_LOGCONFIG(TAG, "  Reference from family code: %s", this->reference_from_address_.c_str());
      ESP_LOGW(TAG, "  WARNING: reference from family code does not match reference !!!");
    }
  }
  ESP_LOGCONFIG(TAG, "  Name: %s", this->name_.c_str());
  if (this->one_wire_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  OneWire ID: %s", this->one_wire_id_->c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  OneWire ID: not defined");
  }
  LOG_ONE_WIRE_DEVICE(this);
}

bool DallasPio::read_state(uint8_t &state, uint8_t pin) {
  this->pin_ = pin;
  if (this->reference_ == "DS2413") {
    // ESP_LOGD(TAG, "DallasPio read_state %u pin %u", state, this->pin_);
    return this->ds2413_get_state_(state);
  } else if (this->reference_ == "DS2406") {
    return this->ds2406_get_state_(state, this->crc_enabled_);
  } else if (this->reference_ == "DS2408") {
    // Action pour DS2408
  } else {
    // Action par défaut
  }
  return true;
}

bool DallasPio::write_state(bool state, uint8_t pin, bool pin_inverted) {
  this->pin_ = pin;
  this->pin_inverted_ = pin_inverted;
  // ESP_LOGW(TAG, "DallasPio write_state %u pin %u pin_ %u", state, pin, this->pin_);
  if (this->reference_ == "DS2413") {
    this->ds2413_write_state_(state);
  } else if (this->reference_ == "DS2406") {
    this->ds2406_write_state_(state, this->crc_enabled_);
  } else if (this->reference_ == "DS2408") {
    // Action for DS2408
  } else {
    // Default action
  }
  return true;
}

/************/
/*  DS2413  */
/************/

bool DallasPio::ds2413_get_state_(uint8_t &state) {
  // Ref DS2413.pdf p6 of 9
  // | b7      b6      b5      b4     |  b3          | b2       |  b1          | b0       |
  // |    Complement of b3 to b0      | PIOB Output  | PIOB Pin | PIOA Output  | PIOA Pin |
  // |                                | Latch State  | State    | Latch State  | State    |
  //  ESP_LOGCONFIG(TAG, "Dallas PIO Binary Sensor Update !");
  uint8_t results;
  bool ok = false;
  // Initialize One-Wire bus for this device
  if (!this->check_address_()) {
    this->status_set_warning();
    return false;
  }
  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return false;
  }
  {
    InterruptLock lock;
    this->send_command_(DALLAS_COMMAND_PIO_ACCESS_READ);
    results = this->bus_->read8();
  }
  ok = (~results & 0x0F) == (results >> 4);
  if (!ok) {
    ESP_LOGW(TAG, "One-Wire checksum error.");
    this->status_set_warning();
    return false;
  }
  // ESP_LOGD(TAG, "results1=%02x", results);
  results &= 0x0F;
  // ESP_LOGD(TAG, "results2=%02x", results);
  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return false;
  }
  switch (this->pin_) {
    case 0x01:  // PIOA
      state = (results & 0x01);
      break;
    case 0x02:  // PIOB
      state = ((results >> 2) & 0x01);
      break;
    default:
      return false;
      break;
  }
  return true;
}  // ds2413_get_state_

void DallasPio::ds2413_write_state_(bool state) {
  // Ref Analog Devices DS2413.pdf p8 of 19
  // | b7 b6 b5 b4 b3 b2 | b1   | b0   |
  // | x  x  x  x  x  x  | PIOB | PIOA |
  uint8_t ack = 0;
  // Initialize One-Wire bus for this device
  if (!this->check_address_()) {
    this->status_set_warning();
    return;
  }
  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return;
  }
  if ((this->pin_ >= 1) && (this->pin_ <= 2)) {
    uint8_t mask = 1 << (this->pin_ - 1);
    // If state different than this->pin_inverted_
    if (state ^ this->pin_inverted_) {
      this->pio_output_register_ &= ~mask;  // set bit bx to 0
    } else {
      this->pio_output_register_ |= mask;  // set bx to 1
    }
  } else {
    return;
  }
  {
    InterruptLock lock;
    this->send_command_(DALLAS_COMMAND_PIO_ACCESS_WRITE);
    this->bus_->write8(this->pio_output_register_);   // Send data
    this->bus_->write8(~this->pio_output_register_);  // Invert data and resend
    ack = this->bus_->read8();                        // 0xAA=success, 0xFF=failure
  }
  if (ack == DALLAS_COMMAND_PIO_ACK_SUCCESS) {
    this->bus_->read8();  // Read the PIOA PIOB status byte
  } else {
    return;
  }
  if (!this->bus_->reset()) {  // reset to end command
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return;
  }
}  // ds2413_write_state

/************/
/*  DS2406  */
/************/

bool DallasPio::ds2406_get_state_(uint8_t &state, bool use_crc = false) {
  // Ref Dallas Semiconductor MAXIM DS2406.pdf
  //     Dallas Semiconductor Application Note 27 app27.pdf
  // CHANNEL CONTROL BYTE 1
  // b7     b6     b5     b4     b3     b2     b1     b0
  // ALR    IM     TOG    IC     CHS1   CHS0   CRC1   CRC0
  // ALR=0 & IM=1, TOG=0 & IC=0 : do not change activity latch, read all bits from the selected channel & async mode
  // CHANNEL CONTROL BYTE 2
  // reserved for future development, must always be 0xFF
  // CHANNEL INFO BYTE
  // b7          b6           b5        b4        b3      b2      b1           b0
  // Supply      Number of    PIOB      PIOA      PIOB    PIOA    PIOB         PIOA
  // Indication  Channels     Activity  Activity  Sensed  Sensed  Channel      Channel
  // 0 = no      0 = channel  Latch     Latch     Level   Level   Flip-Flop Q  Flip-Flop Q
  // supply      A only
  // Please note :
  // CRC1 CRC0
  // 0    0      : CRC mode 0 = CRC disabled (no CRC at all)
  // 0    1      : CRC mode 1 = CRC after every byte

  uint8_t channel_control_byte_1;
  uint8_t channel_control_byte_2 = 0xFF;
  uint8_t channel_info_byte;
  uint8_t received_crc = 0;
  const char *str_pio = nullptr;
  // Initialize One-Wire bus for this device
  if (!this->check_address_()) {
    this->status_set_warning();
    return false;
  }
  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return false;
  }
  switch (this->pin_) {
    case 0x01:                                                     // PIOA
      channel_control_byte_1 = use_crc ? 0b10000101 : 0b10000100;  // ALR=1, CHS0=1, CRC mode 1 or mode 0
      break;
    case 0x02:                                                     // PIOB
      channel_control_byte_1 = use_crc ? 0b10001001 : 0b10001000;  // ALR=1, CHS1=1, CRC mode 1 or mode 0
      break;
    default:
      return false;
      break;
  }
  {
    InterruptLock lock;
    this->send_command_(DALLAS_COMMAND_PIO_ACCESS_READ);
    // write CHANNEL CONTROL BYTE 1
    this->bus_->write8(channel_control_byte_1);
    // write CHANNEL CONTROL BYTE 2
    this->bus_->write8(channel_control_byte_2);
    // read CHANNEL INFO BYTE
    channel_info_byte = this->bus_->read8();
    if (use_crc) {
      received_crc = this->bus_->read8();
    }
  }
  if (use_crc) {
    ESP_LOGD(TAG, "CRC for Channel Info Byte: 0x%04X", use_crc);
    if (!this->ds2406_verify_crc_(channel_control_byte_1, channel_control_byte_2, channel_info_byte, received_crc)) {
      ESP_LOGW(TAG, "CRC verification failed!");
      this->status_set_warning();
      return false;
    }
  }

  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return false;
  }
  // read CHANNEL INFO BYTE
  bool pio_flipflop;
  bool pio_sensed_level;
  bool pio_activity_latch;
  const bool has_channel_b = channel_info_byte & 0x40;
  const bool has_supply = channel_info_byte & 0x80;
  if (!has_supply) {
    ESP_LOGW(TAG, "DS2406 has no supply");
    this->status_set_warning();
    return false;
  }
  if ((this->pin_ == 0x02) && (!has_channel_b)) {
    ESP_LOGW(TAG, "DS2406 has no channel PIOB");
    this->status_set_warning();
    return false;
  }
  switch (this->pin_) {
    case 0x01:  // PIOA
      pio_flipflop = channel_info_byte & 0x01;
      pio_sensed_level = channel_info_byte & 0x04;
      pio_activity_latch = channel_info_byte & 0x10;
      str_pio = "PIOA";
      break;
    case 0x02:  // PIOB
      pio_flipflop = channel_info_byte & 0x02;
      pio_sensed_level = channel_info_byte & 0x08;
      pio_activity_latch = channel_info_byte & 0x20;
      str_pio = "PIOB";
      break;
    default:
      ESP_LOGW(TAG, "DallasPio DS2406 pin %u does not exist !", this->pin_);
      this->status_set_warning();
      return false;
      break;
  }
  if (pio_flipflop == 0) {
    ESP_LOGW(TAG, "DallasPio DS2406 PIO flipflop must be 1 to read %s", str_pio);
    this->status_set_warning();
    return false;
  }
  state = pio_sensed_level;
  return true;
}  // ds2406_get_state_

void DallasPio::ds2406_write_state_(bool state, bool use_crc = false) {
  // Ref Dallas Semiconductor MAXIM DS2406.pdf
  //     Dallas Semiconductor Application Note 27 app27.pdf
  // CHANNEL CONTROL BYTE 1
  // b7     b6     b5     b4     b3     b2     b1     b0
  // ALR    IM     TOG    IC     CHS1   CHS0   CRC1   CRC0
  // ALR=0 & IM=0, TOG=0 & IC=0 : do not change activity latch, write all bits to the selected channel & async mode
  // CHS1=01 CHS0=1 : PIOA, CHS1=1 CHS0=0 : PIOB
  // CHANNEL CONTROL BYTE 2
  // reserved for future development, must always be 0xFF
  // CHANNEL INFO BYTE
  // b7          b6           b5        b4        b3      b2      b1           b0
  // Supply      Number of    PIOB      PIOA      PIOB    PIOA    PIOB         PIOA
  // Indication  Channels     Activity  Activity  Sensed  Sensed  Channel      Channel
  // 0 = no      0 = channel  Latch     Latch     Level   Level   Flip-Flop Q  Flip-Flop Q
  // supply      A only
  // Initialize One-Wire bus for this device
  if (!this->check_address_()) {
    this->status_set_warning();
    return;
  }
  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return;
  }
  // ALR=0,IM=0,TOG=0,IC=0, PIOA:CHS1=0,CHS0=1, PIOB:CHS1=1,CHS0=0, CRC1=0, CRC0=0
  uint8_t channel_control_byte_1;
  switch (this->pin_) {
    case 0x01:  // PIOA
      channel_control_byte_1 = use_crc ? 0b00000101 : 0b00000100;
      break;
    case 0x02:  // PIOB
      channel_control_byte_1 = use_crc ? 0b00001001 : 0b00001000;
      break;
    default:
      return;
      break;
  }
  uint8_t channel_control_byte_2 = 0xFF;
  uint8_t channel_info_byte;
  {
    InterruptLock lock;
    this->send_command_(DALLAS_COMMAND_PIO_ACCESS_READ);
    // write CHANNEL CONTROL BYTE 1
    this->bus_->write8(channel_control_byte_1);
    // write CHANNEL CONTROL BYTE 2
    this->bus_->write8(channel_control_byte_2);
    channel_info_byte = this->bus_->read8();
    this->bus_->write8(state ? 0xFF : 0x00);
  }
  if (!this->bus_->reset()) {
    ESP_LOGW(TAG, "Failed to reset One-Wire bus.");
    this->status_set_warning();
    return;
  }
}  // ds2406_write_state

bool DallasPio::ds2406_verify_crc_(uint8_t control1, uint8_t control2, uint8_t info, uint16_t received_crc) {
  uint16_t computed_crc = 0;
  this->crc_reset_();
  this->crc_shift_byte_(DALLAS_COMMAND_PIO_ACCESS_READ);
  this->crc_shift_byte_(control1);
  this->crc_shift_byte_(control2);
  this->crc_shift_byte_(info);
  computed_crc = this->crc_read_();

  ESP_LOGD(TAG, "Computed CRC: 0x%04X", computed_crc);
  return computed_crc == received_crc;
}

/************/
/*  DS2408  */
/************/

bool DallasPio::ds2408_get_state_(uint8_t &state, bool use_crc = false) {
  ESP_LOGW(TAG, "DallasPio DS2408 : ds2408_get_state_ to be implemented");
  this->status_set_warning();
  return false;
}

void DallasPio::ds2408_write_state_(bool state, bool use_crc = false) {
  ESP_LOGW(TAG, "DallasPio DS2408 : ds2408_write_state_ to be implemented");
  this->status_set_warning();
}

void DallasPio::crc_reset_() { this->crc_ = 0x0000; }

void DallasPio::crc_shift_byte_(uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    bool mix = (this->crc_ ^ byte) & 0x01;
    this->crc_ >>= 1;
    if (mix)
      this->crc_ ^= 0xA001;
    byte >>= 1;
  }
}

uint16_t DallasPio::crc_read_() const { return this->crc_; }

}  // namespace dallas_pio
}  // namespace esphome
