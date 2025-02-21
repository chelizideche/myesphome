#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

/// @brief NEC protocol code types
enum class NECCodeType : uint8_t {
  FRAME_WITH_REPEATS,  ///< Frame with address, command and repeats
  REPEATS_ONLY         ///< Repeat code without address and command
};

/// @brief Struct to store NEC protocol data.
/// @details This structure represents a decoded NEC infrared signal. It includes fields for
///          address, command, repeat count, and type, supporting both full frames and repeat codes.
struct NECData {
  /// @brief NEC address field, stored as a 16-bit value.
  /// @details The NEC protocol supports two addressing modes:
  ///          - **Standard NEC (8-bit addressing)**: The `address_upper` field is the logical inverse of
  ///          `address_lower`.
  ///          - **Extended NEC (16-bit addressing)**: Both `address_lower` and `address_upper` contain independent
  ///          values. If the upper byte is not the inverse of the lower byte, it is treated as an extended NEC address
  ///          in logs.
  union {
    uint16_t address;  ///< Full 16-bit NEC address (used in extended NEC mode).
    struct {
      uint8_t address_lower;  ///< Lower 8 bits of the address.
      uint8_t address_upper;  ///< Upper 8 bits of the address (inverse of lower in standard NEC).
    };
  };

  /// @brief NEC command field, always stored as a 16-bit value.
  /// @details The NEC protocol defines an **8-bit command**, but it is transmitted as **16 bits**, where:
  ///          - `command_lower` holds the actual 8-bit command.
  ///          - `command_upper` is its logical inverse.
  ///          Unlike some implementations that automatically compute the inverse, this protocol requires both values
  ///          to be explicitly provided. This ensures support for **non-standard NEC signals** where the inverse byte
  ///          may not strictly adhere to the NEC specification.
  union {
    uint16_t
        command;  ///< Full 16-bit NEC command (lower 8 bits hold the actual command, upper 8 bits are its inverse).
    struct {
      uint8_t command_lower;  ///< Lower 8 bits (actual command value).
      uint8_t command_upper;  ///< Upper 8 bits (logical inverse of `command_lower` in standard NEC).
    };
  };

  uint16_t repeats;  ///< Number of repeat codes received or transmitted.
  NECCodeType type;  ///< Type of NEC signal (frame with repeats or repeat codes only).

  /// @brief Equality operator for NECData.
  /// @details Compares two NECData instances to determine if they represent the same signal.
  ///          The comparison logic varies based on the NECCodeType:
  ///          - For `REPEATS_ONLY`, only the repeat count is compared.
  ///          - For `FRAME_WITH_REPEATS`, the address, command, and repeat count must all match.
  /// @param[in] rhs The NECData instance to compare with.
  /// @return True if both instances represent the same NEC signal, false otherwise.
  bool operator==(const NECData &rhs) const {
    if (type != rhs.type) {
      return false;
    }

    switch (type) {
      case NECCodeType::REPEATS_ONLY:
        return repeats == rhs.repeats;
      case NECCodeType::FRAME_WITH_REPEATS:
        // Compare repeats to allow differentiation based on button hold duration in the future.
        // This enables NEC data to store information about how long a button was pressed.
        return address == rhs.address && command == rhs.command && repeats == rhs.repeats;
      default:
        return false;
    }
  };
};

/// @brief Predefined single repeat code `NECData` returned by `NECProtocol::decode(RemoteReceiveData)`
static const NECData NEC_REPEAT_CODE_DATA = {{0}, {0}, 1, NECCodeType::REPEATS_ONLY};

/// @brief NECProtocol handles encoding, decoding, and validation of NEC infrared signals.
/// @details This class provides methods to encode and decode NEC IR signals while ensuring compliance
///          with standard and extended NEC formats. It also includes validation functions for NEC addresses
///          and commands, as well as debugging utilities for structured logging of decoded signals.
///
/// ## **Decoding Behavior**
/// The current implementation does **not** support returning a single `NECData` structure containing both
/// the initial frame and its associated repeat codes. Instead:
/// - **Frame decoding**: When a `FRAME_WITH_REPEATS` signal type is received, it is decoded with `repeats = 0`.
/// - **Repeat code handling**: Each repeat code (`REPEATS_ONLY`) is decoded **individually**, returning
///   `NEC_REPEAT_CODE_DATA` for every repeat code received after the initial frame.
///
/// This means the user must handle frame and repeat codes separately, as repeat signals are not appended
/// to the original frame but instead processed as independent messages.
///
class NECProtocol : public RemoteProtocol<NECData> {
 public:
  /// @brief Encodes `NECData` into `RemoteTransmitData`.
  /// @details Generates an NEC IR signal based on the given `NECData`:
  ///          - If `data.type` is `NECCodeType::FRAME_WITH_REPEATS`, it encodes a full NEC frame with address, command,
  ///          stop bit, and the specified number of repeat codes.
  ///          - If `data.type` is `NECCodeType::REPEATS_ONLY`, it encodes just repeat codes without frame.
  /// @note `NECCodeType::REPEATS_ONLY` `data.type` is invalid if `data.repeats` is 0, and no data will be encoded.
  /// @warning A high repeat count may cause a WDT timeout.
  /// @param[out] dst Destination `RemoteTransmitData` for the encoded signal.
  /// @param[in] data NEC data containing type, address, command, and repeat count.
  void encode(RemoteTransmitData *dst, const NECData &data) override;

  /// @brief Decodes `NECData` from `RemoteReceiveData`.
  /// @details This function parses `NECData`, distinguishing between full message frames and repeat codes:
  ///          - If a valid `NECCodeType::FRAME_WITH_REPEATS` is detected, it extracts the address, command.
  ///          - If a `NECCodeType::REPEATS_ONLY` code is detected, it returns a predefined `NEC_REPEAT_CODE_DATA`
  ///          instance.
  /// @note A `NECCodeType::FRAME_WITH_REPEATS` is always decoded with `repeats = 0`, as repeat codes are processed
  /// separately.
  /// @warning If the decoded command is invalid, a warning is logged.
  /// @param[in] src The received IR signal data to be decoded.
  /// @return An `optional<NECData>` containing the decoded NEC data, or an empty optional if decoding fails.
  optional<NECData> decode(RemoteReceiveData src) override;

  void dump(const NECData &data) override;

  /// @brief Determines if the NEC address follows the extended (16-bit) format.
  /// @details In standard NEC (8-bit addressing), `address_upper` should be the logical inverse of `address_lower`.
  ///          If this condition is not met, the address is considered extended (16-bit NEC mode).
  /// @param[in] data The NECData structure containing the address.
  /// @return True if the address is in extended (16-bit) format, false if it follows standard (8-bit) NEC addressing.
  bool is_extended(const NECData &data) const {
    return data.address_lower != static_cast<uint8_t>(~data.address_upper);
  }

  /// @brief Validates whether the NEC command follows the standard format.
  /// @details The NEC protocol requires the upper 8 bits of the command (`command_upper`)
  ///          to be the logical inverse of the lower 8 bits (`command_lower`).
  /// @param[in] data The NECData structure containing the command.
  /// @return True if the command follows the NEC specification, false otherwise.
  bool is_command_valid(const NECData &data) const {
    return data.command_lower == static_cast<uint8_t>(~data.command_upper);
  }

  /// @brief Generates a formatted debug string describing the NEC signal type and its fields.
  /// @details This function constructs a human-readable representation of an NEC signal,
  ///          including its type, address, command, validity, and repeat count.
  ///          - **FRAME_WITH_REPEATS**: Displays address format (8-bit or 16-bit) and command details.
  ///          - **REPEATS_ONLY**: Indicates a repeat code without address/command details.
  ///          - **Unknown Type**: Fallback case for unrecognized NEC types.
  /// @param[in] data The `NECData` structure containing the decoded NEC signal.
  /// @return A formatted string describing the NEC signal type and its fields.
  std::string get_protocol_type_and_fields(const NECData &data) const;
};

/// @brief NEC-based binary sensor for detecting button presses via infrared signals.
/// @details This sensor listens for NEC IR signals and determines if they match a predefined frame.
///          It supports detecting both single button presses and long presses using NEC repeat codes.
///          - A new frame signal sets the sensor to "on".
///          - Repeat codes keep the sensor "on" until they stop arriving.
///          - If no repeat codes arrive within a set timeout, the sensor turns "off".
class NECBinarySensor : public RemoteReceiverBinarySensor<NECProtocol> {
 public:
  NECBinarySensor() : RemoteReceiverBinarySensor<NECProtocol>() {}

  /// @brief Checks if the received IR signal matches the expected NEC frame.
  /// @details Determines if the incoming NEC signal corresponds to the stored reference data.
  ///          - If the sensor is **not waiting for a repeat code**, only a matching frame enables detection.
  ///          - If the sensor **is waiting for a repeat code**, only a valid repeat code keeps it active.
  ///          - If neither condition is met, the sensor stops waiting and turns off.
  /// @param[in] src The received IR data to analyze.
  /// @return True if the received signal matches the expected NEC data, false otherwise.
  bool matches(RemoteReceiveData src) override;

  /// @brief Processes incoming IR data and manages the sensor state.
  /// @details
  ///   - Calls `matches(src)` to determine if the signal matches the expected NEC frame.
  ///   - If a match is found, the sensor enters the "on" state.
  ///   - A timeout is started to turn the sensor "off" if no further repeat codes arrive.
  /// @note The sensor automatically turns "off" when the timeout expires.
  /// @param[in] src The received IR data to process.
  /// @return True if a match is found and the sensor state is updated, false otherwise.
  bool on_receive(RemoteReceiveData src) override;

 protected:
  /// @brief Timeout duration (in milliseconds) for detecting repeat codes before turning off.
  uint8_t repeat_timeout_ms_{130};

  /// @brief Tracks whether the sensor is currently expecting a repeat code.
  bool waiting_for_repeat_code_{false};
};

using NECTrigger = RemoteReceiverTrigger<NECProtocol>;
using NECDumper = RemoteReceiverDumper<NECProtocol>;

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
