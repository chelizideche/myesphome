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
  /// @retval false If types are different or unknown
  /// @retval true If type is `NECCodeType::REPEAT` and repeats are the same
  /// @retval true If type is `NECCodeType::FRAME` and fields are the same
  bool operator==(const NECData &rhs) const {
    if (type != rhs.type) {
      return false;
    }

    switch (type) {
      case NECCodeType::FRAME:
        return address == rhs.address && command == rhs.command && repeats == rhs.repeats;
      case NECCodeType::REPEAT:
        return repeats == rhs.repeats;
      default:
        return false;
    }
  };
};

class NECProtocol : public RemoteProtocol<NECData> {
 public:
  void encode(RemoteTransmitData *dst, const NECData &data) override;
  optional<NECData> decode(RemoteReceiveData src) override;
  void dump(const NECData &data) override;

  std::string get_protocol_type_and_fields(const NECData &data) const;
  bool is_extended(const NECData &data) const {
    return data.address_lower != static_cast<uint8_t>(~data.address_upper);
  }
  bool is_command_valid(const NECData &data) const {
    return data.command_lower == static_cast<uint8_t>(~data.command_upper);
  }
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
