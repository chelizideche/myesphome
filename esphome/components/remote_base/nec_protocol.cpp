#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nec";

// NEC IR Protocol (old URL https://techdocs.altium.com/display/FPGA/NEC%2bInfrared%2bTransmission%2bProtocol)
// Protocol Reference:
// https://files.doc.altium.com/sites/default/files/wiki_attachments/299480/Legacy%20-%20FPGA%20Design.pdf
// https://www.infineon.com/dgdl/Infineon-AN2023-03_Infrared_Remote_Control_and_Saving_Last_Speed_Setting-ApplicationNotes-v01_00-EN.pdf?fileId=8ac78c8c8d1b852e018d21ff0aa71feb
// // NOLINT https://www.renesas.com/ja/document/apn/1184-remote-control-ir-receiver-decoder

// Timing constants in microseconds
static const uint32_t AGC_HIGH_US = 9000;            // AGC burst: 9ms HIGH
static const uint32_t LONG_PAUSE_LOW_US = 4500;      // After AGC burst: 4.5ms LOW
static const uint32_t SHORT_PAUSE_LOW_US = 2250;     // After AGC burst: 2.25ms LOW
static const uint32_t BIT_HIGH_US = 562;             // Bit HIGH duration: 562.5µs
static const uint32_t BIT_ONE_LOW_US = 1687;         // Logic '1': 562.5µs HIGH + 1687.5µs LOW
static const uint32_t BIT_ZERO_LOW_US = 562;         // Logic '0': 562.5µs HIGH + 562.5µs LOW
static const uint32_t SPACE_INTER_FRAME_US = 40000;  // Inter-frame space: 40ms
static const uint32_t SPACE_AGC_REPEAT_US = 96187;   // AGC Repeat space: ~96.1875ms

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  ESP_LOGD(TAG, "Encoding %s", this->get_protocol_type_and_fields(data).c_str());

  if (data.repeats > 20) {
    ESP_LOGW(TAG, "High repeat count may cause WDT timeout.");
  }

  if (data.repeats == 0 && data.type != NECCodeType::FRAME_WITH_REPEATS) {
    ESP_LOGE(TAG, "NECData repeat count must be greater than 0 for type NECCodeType::FRAME_WITH_REPEATS.");
  }

  // Repeat codes (4 per repeat)
  uint32_t dst_len = data.repeats * 4;
  if (data.type == NECCodeType::FRAME_WITH_REPEATS) {
    dst_len += 2;   // AGC Header (2)
    dst_len += 32;  // Address bits (32)
    dst_len += 32;  // Command bits (32)
    dst_len += 2;   // Stop bit (2)
  }
  dst->reserve(dst_len);
  dst->set_carrier_frequency(38222);

  if (data.type == NECCodeType::FRAME_WITH_REPEATS) {
    // Send the AGC Header (start of frame)
    dst->item(AGC_HIGH_US, LONG_PAUSE_LOW_US);

    // Encode Address
    for (uint16_t mask = 1; mask; mask <<= 1) {
      if (data.address & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);  // Logic '1'
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);  // Logic '0'
      }
    }

    // Encode Command
    for (uint16_t mask = 1; mask; mask <<= 1) {
      if (data.command & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);  // Logic '1'
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);  // Logic '0'
      }
    }

    // Stop bit to end the message frame
    dst->mark(BIT_HIGH_US);

    // Space between message frame and first repeat code
    if (data.repeats > 0) {
      dst->space(SPACE_INTER_FRAME_US);
    }
  }

  // Send AGC Repeat Codes if requested
  for (uint16_t repeats = 0; repeats < data.repeats; ++repeats) {
    // AGC Repeat header (shorter version of the initial AGC header)
    dst->item(AGC_HIGH_US, SHORT_PAUSE_LOW_US);  // Shortened AGC header
    dst->mark(BIT_HIGH_US);                      // Stop bit to complete the repeat code

    // Add space after repeat code, except after the final repeat
    if (repeats < data.repeats - 1) {
      dst->space(SPACE_AGC_REPEAT_US);
    }
  }
}

optional<NECData> NECProtocol::decode(RemoteReceiveData src) {
  NECData data{
      .address = 0,
      .command = 0,
      .repeats = 0,  // Start with 0, as the first frame is counted explicitly
      .type = NECCodeType::FRAME_WITH_REPEATS,
  };

  // Validate the AGC header (start of frame or repeat code)
  if (!src.expect_mark(AGC_HIGH_US)) {
    return {};
  }

  // Validate the short pause (repeat code)
  if (src.expect_space(SHORT_PAUSE_LOW_US)) {
    // Validate the stop bit of repeat code
    if (!src.expect_mark(BIT_HIGH_US)) {
      return {};
    }

    // Repeat code received
    return NEC_REPEAT_CODE_DATA;
  }

  // Validate the long pause (message frame)
  if (!src.expect_space(LONG_PAUSE_LOW_US)) {
    return {};
  }

  // Validate address bits
  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.address |= mask;  // Logic '1'
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      // Logic '0', since the address is already initialized with 0, `data.address &= ~mask;` is not needed
    } else {
      return {};
    }
  }

  // Validate command bits
  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.command |= mask;  // Logic '1'
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      // Logic '0', since the command is already initialized with 0, `data.command &= ~mask;` is not needed
    } else {
      return {};
    }
  }

  // Validate the final stop bit (end of the message frame)
  if (!src.expect_mark(BIT_HIGH_US)) {
    return {};
  }

  // Message frame received, `data.type = NECCodeType::FRAME_WITH_REPEATS` is already set
  if (!this->is_command_valid(data)) {
    ESP_LOGW(TAG, "Decoded command invalid: 0x%04X", data.command);
  }

  ESP_LOGV(TAG, "Decoded %s", this->get_protocol_type_and_fields(data).c_str());

  return data;
}

void NECProtocol::dump(const NECData &data) {
  ESP_LOGI(TAG, "Received %s", this->get_protocol_type_and_fields(data).c_str());
}

std::string NECProtocol::get_protocol_type_and_fields(const NECData &data) const {
  std::string debug_message = "NEC ";
  switch (data.type) {
    case NECCodeType::FRAME_WITH_REPEATS:
      debug_message += str_sprintf("Frame (%u-bit address)", this->is_extended(data) ? 16 : 8);
      break;
    case NECCodeType::REPEATS_ONLY:
      debug_message += "Repeat Code:";
      break;
    default:
      debug_message += "Unknown";
  }

  if (data.type != NECCodeType::REPEATS_ONLY) {
    debug_message += ": address=0x";
    if (this->is_extended(data)) {
      debug_message += str_sprintf("%04X", data.address);
    } else {
      debug_message += str_sprintf("%02X, address#=0x%02X", data.address_lower, data.address_upper);
    }

    debug_message += str_sprintf(", command=0x%02X, command#=0x%02X, command_valid=%s,", data.command_lower,
                                 data.command_upper, YESNO(this->is_command_valid(data)));
  }

  debug_message += str_sprintf(" repeats=%" PRIu16, data.repeats);

  return debug_message;
}

bool NECBinarySensor::matches(RemoteReceiveData src) {
  auto proto = NECProtocol();
  auto res = proto.decode(src);

  if (!res.has_value()) {
    return false;
  }

  bool last_waiting_state = this->waiting_for_repeat_code_;

  switch (res.value().type) {
    case NECCodeType::FRAME_WITH_REPEATS:
      // Set waiting to true only if currently not waiting and this is our desired frame.
      this->waiting_for_repeat_code_ = (!this->waiting_for_repeat_code_ && (res.value() == this->data_));
      break;

    case NECCodeType::REPEATS_ONLY:
      // Stay waiting only if we were already waiting and we got a valid repeat code.
      this->waiting_for_repeat_code_ = (this->waiting_for_repeat_code_ && (res.value() == NEC_REPEAT_CODE_DATA));
      break;

    default:
      this->waiting_for_repeat_code_ = false;
      break;
  }

  if (last_waiting_state && !this->waiting_for_repeat_code_) {
    this->publish_state(false);
    this->cancel_timeout("repeat");
  }

  return this->waiting_for_repeat_code_;
}

bool NECBinarySensor::on_receive(RemoteReceiveData src) {
  if (!this->matches(src)) {
    return false;
  }

  this->publish_state(true);
  this->set_timeout("repeat", this->repeat_timeout_ms_, [this]() {
    this->waiting_for_repeat_code_ = false;
    this->publish_state(false);
  });

  return true;
}
}  // namespace remote_base
}  // namespace esphome
