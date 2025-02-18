#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

/// @brief NEC protocol frame types
enum class NECCodeType : uint8_t {
  FRAME,  ///< Frame with address, command and repeats
  REPEAT  ///< Repeat code without address and command
};

/// @brief Struct to store NEC protocol data
struct NECData {
  union {
    uint16_t address;
    struct {
      uint8_t address_lower;
      uint8_t address_upper;
    };
  };

  union {
    uint16_t command;
    struct {
      uint8_t command_lower;
      uint8_t command_upper;
    };
  };

  uint16_t repeats;
  NECCodeType type;

  /// @brief Equality operator for NECData
  /// @return True if type and relevant fields match, false otherwise.
  bool operator==(const NECData &rhs) const {
    if (type != rhs.type) {
      return false;
    }

    switch (type) {
      case NECCodeType::REPEAT:
        return repeats == rhs.repeats;
      case NECCodeType::FRAME:
        return address == rhs.address && command == rhs.command && repeats == rhs.repeats;
      default:
        return false;
    }
  };
};

static const NECData NEC_REPEAT_CODE_DATA = {0, 0, 1, NECCodeType::REPEAT};

class NECProtocol : public RemoteProtocol<NECData> {
 public:
  /// @brief Encodes `NECData` into `RemoteTransmitData`.
  /// @details Generates an NEC IR signal based on the given `NECData`:
  ///          - If `data.type` is `NECCodeType::FRAME`, it encodes a full NEC frame with address, command, stop bit,
  ///          and the specified number of repeat codes.
  ///          - If `data.type` is `NECCodeType::REPEAT`, it encodes just repeat codes without frame.
  /// @note `NECCodeType::REPEAT` `data.type` is invalid if `data.repeats` is 0, and no data will be encoded.
  /// @warning A high repeat count may cause a WDT timeout.
  /// @param[out] dst Destination `RemoteTransmitData` for the encoded signal.
  /// @param[in] data NEC data containing type, address, command, and repeat count.
  void encode(RemoteTransmitData *dst, const NECData &data) override;
  /// @brief Decodes `NECData` from `RemoteReceiveData`.
  /// @details This function parses `NECData`, distinguishing between full message frames and repeat codes:
  ///          - If a valid `NECCodeType::FRAME` is detected, it extracts the address, command.
  ///          - If a `NECCodeType::REPEAT` code is detected, it returns a predefined `NEC_REPEAT_CODE_DATA` instance.
  /// @note A `NECCodeType::FRAME` is always decoded with `repeats = 0`, as repeat codes are processed separately.
  /// @warning If the decoded command is invalid, a warning is logged.
  /// @param[in] src The received IR signal data to be decoded.
  /// @return An `optional<NECData>` containing the decoded NEC data, or an empty optional if decoding fails.
  optional<NECData> decode(RemoteReceiveData src) override;
  void dump(const NECData &data) override;

  /// @brief Check if the address is extended (16-bits)
  /// @return True if the address is extended, false otherwise.
  bool is_extended(const NECData &data) const {
    return data.address_lower != static_cast<uint8_t>(~data.address_upper);
  }
  /// @brief Check if the 8-bit command is valid
  /// @return True if the command is valid, false otherwise.
  bool is_command_valid(const NECData &data) const {
    return data.command_lower == static_cast<uint8_t>(~data.command_upper);
  }
  /// @brief Get the protocol type and fields from the NECData struct as std::string
  std::string get_protocol_type_and_fields(const NECData &data) const;
};

DECLARE_REMOTE_PROTOCOL(NEC)

template<typename... Ts> class NECAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint16_t, command)
  TEMPLATABLE_VALUE(uint16_t, repeats)
  TEMPLATABLE_VALUE(NECCodeType, type)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    NECData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    data.repeats = this->repeats_.value(x...);
    data.type = this->type_.value(x...);
    NECProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
