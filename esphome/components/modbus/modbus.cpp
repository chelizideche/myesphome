#include "modbus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace modbus {

static const char *const TAG = "modbus";

void Modbus::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }
  // 3.5 characters * 11 bits per character * 1000ms/sec / (bits/sec) (Standard modbus frame delay)
  this->frame_delay_ms_ = (3.5 * 11 * 1000 / this->parent_->get_baud_rate()) + 1;
  // 120 characters (default RxFIFOBuffer in HardwareSerial)
  this->long_rx_buffer_delay_ms_ = (120 * 11 * 1000 / this->parent_->get_baud_rate()) + 1;
  if (this->frame_delay_ms_ < 2)
    this->frame_delay_ms_ = 2;  // 1750us minimium per spec - rounded up to 2ms.
}

void Modbus::loop() {
  // First process all available incoming data.
  this->receive_and_parse_modbus_bytes_();

  // If the response frame is finished (including interframe delay) - we timeout.
  // UART splits rx_buffer into 120 character chunks by default.
  // We add in the long_rx_buffer_delay_ms_ to avoid timeing out when waiting for the buffer to transfer.
  if (millis() - last_modbus_byte_ > frame_delay_ms_ + (rx_buffer_.size() > 100 ? long_rx_buffer_delay_ms_ : 0)) {
    clear_rx_buffer_("timeout");
  }

  // If we're past the send_wait_time timeout and response buffer doesn't have the start of the expected response
  if (waiting_for_response_ != 0 && millis() - last_send_ > send_wait_time_ &&
      (rx_buffer_.empty() || rx_buffer_[0] != waiting_for_response_)) {
    ESP_LOGV(TAG, "Stop waiting for response from %d %dms after last send", waiting_for_response_, millis() - last_send_);
    waiting_for_response_ = 0;
  }

  // If there's no response pending and there's commands in the buffer
  if (!tx_blocked() && !this->tx_buffer_.empty()) {
    set_timeout("send_next_frame", 0, [this]() { this->send_next_frame_(); });
  }
}

bool Modbus::tx_blocked() {
  const uint32_t now = millis();

  return available() || !rx_buffer_.empty() || (waiting_for_response_ != 0) ||
         (now - last_send_ < frame_delay_ms_ + (role == ModbusRole::CLIENT ? turnaround_delay_ms_ : 0)) ||
         (now - last_modbus_byte_ < frame_delay_ms_ + (role == ModbusRole::CLIENT ? turnaround_delay_ms_ : 0));
}

bool Modbus::tx_buffer_empty() { return tx_buffer_.empty(); }

void Modbus::receive_and_parse_modbus_bytes_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (rx_buffer_.empty()) {
      ESP_LOGV(TAG, "Modbus received first Byte %d (0X%x) %dms after last send", byte, byte, millis() - last_send_);
    } else {
      ESP_LOGVV(TAG, "Modbus received Byte %d (0X%x) %dms after last send", byte, byte, millis() - last_send_);
    }

    // If the bytes in the rx buffer do not parse, clear out the buffer
    if (!this->parse_modbus_byte_(byte)) {
      clear_rx_buffer_("parse failed");
    }
    last_modbus_byte_ = millis();
  }
}

bool Modbus::parse_modbus_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  const uint8_t *raw = &this->rx_buffer_[0];

  // Byte 0: modbus address (match all)
  if (at == 0)
    return true;
  uint8_t address = raw[0];
  uint8_t function_code = raw[1];
  // Byte 2: Size (with modbus rtu function code 4/3)
  // See also https://en.wikipedia.org/wiki/Modbus
  if (at == 2)
    return true;

  uint8_t data_len = raw[2];
  uint8_t data_offset = 3;

  // Per https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf Ch 5 User-Defined function codes
  if (((function_code >= 65) && (function_code <= 72)) || ((function_code >= 100) && (function_code <= 110))) {
    // Handle user-defined function, since we don't know how big this ought to be,
    // ideally we should delegate the entire length detection to whatever handler is
    // installed, but wait, there is the CRC, and if we get a hit there is a good
    // chance that this is a complete message ... admittedly there is a small chance is
    // isn't but that is quite small given the purpose of the CRC in the first place

    // Fewer than 2 bytes can't calc CRC
    if (at < 2)
      return true;

    data_len = at - 2;
    data_offset = 1;

    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);

    if (computed_crc != remote_crc)
      return true;

    ESP_LOGD(TAG, "Modbus user-defined function %02X found", function_code);

  } else {
    // data starts at 2 and length is 4 for read registers commands
    if (this->role == ModbusRole::SERVER && (function_code == 0x3 || function_code == 0x4)) {
      data_offset = 2;
      data_len = 4;
    }

    // the response for write command mirrors the requests and data starts at offset 2 instead of 3 for read commands
    if (function_code == 0x5 || function_code == 0x06 || function_code == 0xF || function_code == 0x10) {
      data_offset = 2;
      data_len = 4;
    }

    // Error ( msb indicates error )
    // response format:  Byte[0] = device address, Byte[1] function code | 0x80 , Byte[2] exception code, Byte[3-4] crc
    if ((function_code & 0x80) == 0x80) {
      data_offset = 2;
      data_len = 1;
    }

    // Byte data_offset..data_offset+data_len-1: Data
    if (at < data_offset + data_len)
      return true;

    // Byte 3+data_len: CRC_LO (over all bytes)
    if (at == data_offset + data_len)
      return true;

    // Byte data_offset+len+1: CRC_HI (over all bytes)
    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);
    if (computed_crc != remote_crc) {
      if (this->disable_crc_) {
        ESP_LOGD(TAG, "Modbus CRC Check failed, but ignored! %02X!=%02X  %s %dms after last send", computed_crc,
                 remote_crc, format_hex_pretty(rx_buffer_).c_str(), millis() - last_send_);
      } else {
        ESP_LOGW(TAG, "Modbus CRC Check failed! %02X!=%02X %s %dms after last send", computed_crc, remote_crc,
                 format_hex_pretty(rx_buffer_).c_str(), millis() - last_send_);
        return false;
      }
    }
  }
  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + data_offset + data_len);
  bool found = false;
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      // Is it an error response?
      if ((function_code & 0x80) == 0x80) {
        ESP_LOGD(TAG, "Modbus error function code: 0x%X exception: %d", function_code, raw[2]);
        if (waiting_for_response_ == address) {
          device->on_modbus_error(function_code & 0x7F, raw[2]);
        } else {
          // Ignore modbus exception not related to a pending command
          ESP_LOGD(TAG, "Ignoring Modbus error - not expecting a response");
        }
      } else if (this->role == ModbusRole::SERVER && (function_code == 0x3 || function_code == 0x4)) {
        device->on_modbus_read_registers(function_code, uint16_t(data[1]) | (uint16_t(data[0]) << 8),
                                         uint16_t(data[3]) | (uint16_t(data[2]) << 8));
      } else {
        device->on_modbus_data(data);
      }
      found = true;
    }
  }

  if (!found) {
    ESP_LOGW(TAG, "Got Modbus frame from unknown address 0x%02X! ", address);
  }

  clear_rx_buffer_("parse succeeded");

  if (waiting_for_response_ == address)
    waiting_for_response_ = 0;

  return true;
}

void Modbus::send_next_frame_() {
  if (this->tx_buffer_.size() == 0)
    return;

  if (tx_blocked())
    return;

  std::vector<uint8_t> data = this->tx_buffer_.front();

  if (this->role == ModbusRole::CLIENT) {
    waiting_for_response_ = data[0];
  }

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(data);
  this->flush();

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);

  this->tx_buffer_.pop_front();

  last_send_ = millis();
  ESP_LOGV(TAG, "Modbus write: %s at %d", format_hex_pretty(data).c_str(), last_send_);

  if (!this->tx_buffer_.empty()) {
    ESP_LOGV(TAG, "Modbus write queue contains %d items.", this->tx_buffer_.size());
  }
}

void Modbus::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
  ESP_LOGCONFIG(TAG, "  Turnaround Time: %d ms", this->turnaround_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Frame Delay: %d ms", this->frame_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Long Rx Buffer Delay: %d ms", this->long_rx_buffer_delay_ms_);
  ESP_LOGCONFIG(TAG, "  CRC Disabled: %s", YESNO(this->disable_crc_));
}
float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void Modbus::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                  uint8_t payload_len, const uint8_t *payload) {
  static const size_t MAX_VALUES = 128;

  // Only check max number of registers for standard function codes
  // Some devices use non standard codes like 0x43
  if (number_of_entities > MAX_VALUES && function_code <= 0x10) {
    ESP_LOGE(TAG, "send too many values %d max=%zu", number_of_entities, MAX_VALUES);
    return;
  }

  std::vector<uint8_t> data;
  data.push_back(address);
  data.push_back(function_code);
  if (this->role == ModbusRole::CLIENT) {
    data.push_back(start_address >> 8);
    data.push_back(start_address >> 0);
    if (function_code != 0x5 && function_code != 0x6) {
      data.push_back(number_of_entities >> 8);
      data.push_back(number_of_entities >> 0);
    }
  }

  if (payload != nullptr) {
    if (this->role == ModbusRole::SERVER || function_code == 0xF || function_code == 0x10) {  // Write multiple
      data.push_back(payload_len);  // Byte count is required for write
    } else {
      payload_len = 2;  // Write single register or coil
    }
    for (int i = 0; i < payload_len; i++) {
      data.push_back(payload[i]);
    }
  }

  send_raw(data);
}

// Helper function for lambdas
// Send raw command. Except CRC everything must be contained in payload
void Modbus::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }
  std::vector<uint8_t> data = payload;

  auto crc = crc16(data.data(), data.size());
  data.push_back(crc >> 0);
  data.push_back(crc >> 8);

  for (auto &item : tx_buffer_) {
    if (item == data) {
      ESP_LOGW(TAG, "Duplicate queued command dropped: %s", format_hex_pretty(data).c_str());
      return;
    }
  }

  if (tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
    tx_buffer_.push_back(data);
  } else {
    ESP_LOGW(TAG, "Modbus write buffer full, dropped: %s", format_hex_pretty(data).c_str());
  }
}

void Modbus::clear_rx_buffer_(const std::string &reason) {
  size_t at = this->rx_buffer_.size();
  if (at > 0) {
    ESP_LOGV(TAG, "Clearing buffer of %d bytes - %s %dms after last send", at, reason.c_str(), millis() - last_send_);
    this->rx_buffer_.clear();
  }
}

}  // namespace modbus
}  // namespace esphome
