#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

extern const uint16_t NEC_SPACE_INTER_FRAME_US;  // Inter-frame space
extern const uint32_t NEC_SPACE_AGC_REPEAT_US;   // AGC Repeat space

/// @brief NEC protocol code types used for encoding/decoding and representing NEC infrared signals.
/// @warning Only `NECCodeType::FRAME` and `NECCodeType::REPEAT` are directly encoded during signal generation.
/// @details
/// - **NEC1**: Sends a single full frame (address + command), followed by one or more NEC repeat codes if needed.
/// - **NEC2**: Sends the full frame repeatedly; each transmission includes the complete address and command,
///   with no use of NEC repeat codes. Each retransmission is treated as a new frame.
enum class NECCodeType : uint8_t {
  FRAME,                    ///< A single NEC frame containing address and command.
  NEC1_FRAME_WITH_REPEATS,  ///< NEC1: One full frame followed by repeat codes.
  NEC2_FRAME_WITH_REPEATS,  ///< NEC2: Full frame is retransmitted multiple times; no repeat codes used.
  REPEAT                    ///< NEC repeat code.
};

/// @brief Represents NEC infrared protocol data.
/// @details This struct stores a decoded NEC infrared signal, including its address, command and type.
struct NECData {
  /// @brief NEC address (16-bit).
  /// @details The NEC protocol supports two addressing modes:
  ///          - **Standard NEC (8-bit address)**: `address_bytes.hi` is the inverse of `address_bytes.lo`.
  ///          - **Extended NEC (16-bit address)**: Both bytes contain independent values.
  union {
    uint16_t address;  ///< Full 16-bit NEC address.
    struct {
      uint8_t lo;  ///< Lower 8 bits.
      uint8_t hi;  ///< Upper 8 bits (inverse of lower in standard NEC).
    } address_bytes;
  };

  /// @brief NEC command (16-bit).
  /// @details An NEC command is always 8 bits but transmitted as 16 bits:
  ///          - `command_bytes.lo`: Actual command.
  ///          - `command_bytes.hi`: Logical inverse of `command_bytes.lo` (in standard NEC).
  ///          Some non-standard signals may not follow this inversion rule.
  union {
    uint16_t command;  ///< Full 16-bit NEC command.
    struct {
      uint8_t lo;  ///< Lower 8 bits (actual command).
      uint8_t hi;  ///< Upper 8 bits (logical inverse in standard NEC).
    } command_bytes;
  };

  /// @brief Number of repeat codes.
  /// @note Reserved for future use.
  uint16_t repeats;

  /// @brief Type of NEC signal.
  NECCodeType type;

  /// @brief Compares two `NECData` objects for equality.
  /// @param rhs The `NECData` object to compare against.
  /// @return True if the objects are considered equal based on type-specific fields, `false` otherwise.
  /// @details
  /// - For type `NECCodeType::FRAME`: compares address and command fields.
  /// - For type `NECCodeType::NEC1_FRAME_WITH_REPEATS` and `NEC2_FRAME_WITH_REPEATS`: compares address, command, and
  /// repeats.
  /// - For type `NECCodeType::REPEAT`: always returns `true` if types match (no other fields are compared).
  /// - Returns `false` if types differ.
  bool operator==(const NECData &rhs) const {
    // Types must match
    if (type != rhs.type) {
      return false;
    }

    switch (type) {
      case NECCodeType::FRAME:
        return address == rhs.address && command == rhs.command;
      case NECCodeType::NEC1_FRAME_WITH_REPEATS:
      case NECCodeType::NEC2_FRAME_WITH_REPEATS:
        return address == rhs.address && command == rhs.command && repeats == rhs.repeats;
      case NECCodeType::REPEAT:
        return true;
    }

    // Unknown type
    return false;
  };
};

/// @brief NECProtocol handles encoding, decoding, and validation of NEC infrared signals.
/// @warning Signals are individually decoded with coresponding type. See details below.
/// @details This class provides methods to encode and decode NEC IR signals while ensuring compliance
///          with standard and extended NEC formats. It also includes validation functions for NEC addresses
///          and commands, as well as debugging utilities for structured logging of decoded signals.
///
/// ## **Decoding Behavior**
/// The current implementation does **not** support returning a single `NECData` structure containing both
/// the initial frame and its associated repeat codes. Instead:
/// - **Frame decoding**: When a `NECCodeType::FRAME` signal type is received, it is decoded and returned.
/// - **Repeat code handling**: Each repeat code (`NECCodeType::REPEAT`) is decoded **individually**, returning
///   `NECData` with type `NECCodeType::REPEAT` for every repeat code received.
///
/// This means the user must handle frame and repeat codes separately, as repeat signals are not appended
/// to the original frame but instead processed as independent messages.
///
class NECProtocol : public RemoteProtocol<NECData> {
 public:
  /// @brief Encodes `NECData` into `RemoteTransmitData`.
  /// @details Generates an NEC IR signal based on the given `NECData`:
  ///          - If type is `NECCodeType::FRAME`, it encodes a full NEC frame with address, command,
  ///          stop bit.
  ///          - If type is `NECCodeType::REPEAT`, it encodes just the repeat code.
  /// @warning Function accepts only `NECCodeType::FRAME` and `NECCodeType::REPEAT` types.
  /// @param[out] dst Destination `RemoteTransmitData` for the encoded signal.
  /// @param[in] data `NECData` containing type, address, command.
  void encode(RemoteTransmitData *dst, const NECData &data) override;

  /// @brief Decodes `NECData` from `RemoteReceiveData`.
  /// @details This function parses `NECData`, distinguishing between full message frames and repeat codes:
  ///          - If a valid `NECCodeType::FRAME` is detected, it extracts the address, command and returns `NECData`
  ///          with type `NECCodeType::FRAME`.
  ///          - If a valid `NECCodeType::REPEAT` code is detected, it returns `NECData` with type
  ///          `NECCodeType::REPEAT`.
  /// @note Each frame or repeat code is decoded as a separate `NECData` with corresponding type.
  /// @param[in] src The received IR signal data to be decoded.
  /// @return An `optional<NECData>` containing the decoded `NECData`, or an empty optional if decoding fails.
  optional<NECData> decode(RemoteReceiveData src) override;

  void dump(const NECData &data) override;

  /// @brief Determines if the NEC address follows the extended (16-bit) format.
  /// @details In standard NEC (8-bit addressing), upper address byte should be the logical inverse of lower address
  /// byte.
  ///          If this condition is not met, the address is considered extended (16-bit NEC mode).
  /// @param[in] data The `NECData` structure containing the address.
  /// @return True if the address is in extended (16-bit) format, false if it follows standard (8-bit) NEC addressing.
  static bool is_address_extended(const NECData &data) {
    return data.address_bytes.hi != static_cast<uint8_t>(~data.address_bytes.lo);
  }

  /// @brief Validates whether the NEC command follows the standard format.
  /// @details The NEC protocol requires the upper byte of the command to be the logical inverse of the lower byte.
  ///          If this condition is not met, the command is considered invalid.
  /// @param[in] data The `NECData` structure containing the command.
  /// @return True if the command follows the NEC specification, false otherwise.
  static bool is_command_valid(const NECData &data) {
    return data.command_bytes.hi == static_cast<uint8_t>(~data.command_bytes.lo);
  }

  /// @brief Generates a formatted debug string describing the NEC signal type and its fields.
  /// @details This function constructs a human-readable representation of an NEC signal,
  ///          including its type, address, command, validity, and repeat count.
  ///          - **NEC Frame**: Displays address format (8-bit or 16-bit) and command details.
  ///          - **NEC Repeat Code**: Indicates a repeat code without address/command details.
  ///          - **Unknown**: Fallback case.
  /// @param[in] data The `NECData` structure containing the decoded NEC signal.
  /// @return A formatted string describing the NEC signal type and its fields.
  static std::string get_protocol_type_and_fields_str(const NECData &data);
};

/// @brief NEC-based binary sensor for detecting button presses via infrared signals.
/// @details This sensor listens for NEC IR signals and determines if they match a predefined frame.
///          It supports detecting both single button presses and long presses using NEC repeat codes.
///          - A new frame signal sets the sensor to "on".
///          - Same frame or repeat codes keep the sensor "on" until they stop arriving.
///          - If no frame or repeat codes arrive within a set timeout, the sensor turns "off".
class NECBinarySensor : public RemoteReceiverBinarySensor<NECProtocol> {
 public:
  NECBinarySensor() : RemoteReceiverBinarySensor<NECProtocol>() {}

  void set_repeat_timeout_ms(uint8_t repeat_timeout_ms) { this->repeat_timeout_ms_ = repeat_timeout_ms; }

  /// @brief Checks if the received IR signal matches the expected NEC frame.
  /// @details Determines if the incoming NEC signal corresponds to the stored reference data.
  ///          - If the sensor is **not waiting for a repeat code**, only a matching frame enables detection.
  ///          - If the sensor **is waiting for a repeat code**, only a matching frame or valid repeat code keeps it
  ///          active.
  ///          - If neither condition is met, the sensor stops waiting and turns off.
  /// @param[in] src The received IR data to analyze.
  /// @return True if the received signal matches the expected NEC data, false otherwise.
  bool matches(RemoteReceiveData src) override;

  /// @brief Processes incoming IR data and manages the sensor state.
  /// @details
  ///   - Calls `matches(RemoteReceiveData src)` to determine if the signal matches the expected NEC frame.
  ///   - If a match is found, the sensor enters the "on" state.
  ///   - A timeout is started to turn the sensor "off" if no further frames or repeat codes arrive.
  /// @note The sensor automatically turns "off" when the timeout expires.
  /// @param[in] src The received IR data to process.
  /// @return True if a match is found and the sensor state is updated, false otherwise.
  bool on_receive(RemoteReceiveData src) override;

  void dump_config() override;

 protected:
  /// @brief Timeout duration (in milliseconds) for detecting repeat codes before turning off.
  uint8_t repeat_timeout_ms_{130};

  /// @brief Tracks whether the sensor is currently expecting a repeat code.
  bool currently_on_and_waiting_for_repeat_code_{false};
};

using NECTrigger = RemoteReceiverTrigger<NECProtocol>;
using NECDumper = RemoteReceiverDumper<NECProtocol>;

/// @brief Action class for sending NEC IR signals using templatable parameters.
/// @tparam Ts Parameter pack for templatable arguments.
/// @details
/// This class supports sending NEC1 and NEC2 infrared frames using a `RemoteTransmitter`.
/// The behavior is controlled via the `NECCodeType` selected through the `type` parameter:
/// - `NECCodeType::NEC1_FRAME_WITH_REPEATS`: Sends a single NEC frame followed by NEC1-style repeat codes.
/// - `NECCodeType::NEC2_FRAME_WITH_REPEATS`: Sends the full frame repeatedly (no separate repeat codes).
///
/// @warning `NECCodeType::FRAME` and `NECCodeType::REPEAT` are reserved for internal encoding use only
/// and must not be used as values for the `type` parameter in this action. Doing so will result in no transmission.
template<typename... Ts> class NECAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint16_t, command)
  TEMPLATABLE_VALUE(NECCodeType, type)

  /// @brief Sends the NEC signal based on configured parameters.
  /// @param x... Additional arguments passed to templatable values.
  /// @details
  /// Based on the `type`, this method determines whether to send:
  /// - A single frame (NEC1 or NEC2)
  /// - A frame followed by NEC1-style repeat codes
  /// - Multiple full frames (NEC2 style)
  /// Includes inter-frame spacing and optional delay compensation between frame and repeat code.
  void play(Ts... x) override {
    if (!this->type_.has_value()) {
      return;
    }

    uint32_t send_times = this->send_times_.value_or(x..., 1);
    if (send_times == 0) {
      return;
    }

    // Determine how many full frames to send
    uint32_t frame_send_times = send_times;

    // Accpet only valid NEC code types
    NECCodeType type = this->type_.value(x...);
    switch (type) {
      case NECCodeType::NEC1_FRAME_WITH_REPEATS:
        if (frame_send_times > 1) {
          frame_send_times = 1;
        }
        break;
      case NECCodeType::NEC2_FRAME_WITH_REPEATS:
        break;
      default:
        return;
    }

    // Prepare NEC frame data
    NECData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    data.repeats = send_times - 1;
    data.type = NECCodeType::FRAME;

    // Transmit the full frame
    auto call = this->transmitter_->transmit();
    NECProtocol().encode(call.get_data(), data);
    call.set_send_times(frame_send_times);
    call.set_send_wait(NEC_SPACE_INTER_FRAME_US);
    call.perform();

    if (type == NECCodeType::NEC2_FRAME_WITH_REPEATS) {
      return;
    }

    // Send NEC1 repeats only if needed
    if (send_times == 1) {
      return;
    }

    // Prepare to send NEC1-style repeat codes
    auto start_millis = micros();  // Measure encoding time for delay compensation

    call.get_data()->reset();
    data.type = NECCodeType::REPEAT;
    NECProtocol().encode(call.get_data(), data);
    call.set_send_times(send_times - 1);
    call.set_send_wait(NEC_SPACE_AGC_REPEAT_US);

    // Delay between frame and first repeat to ensure proper signal timing
    auto delay_time = NEC_SPACE_INTER_FRAME_US - (micros() - start_millis);
    if (delay_time <= NEC_SPACE_INTER_FRAME_US) {
      delayMicroseconds(delay_time);
    }
    call.perform();
  }

  /// @brief Encodes the NEC signal into a `RemoteTransmitData` object without transmitting.
  /// @param dst Pointer to the destination `RemoteTransmitData` buffer.
  /// @param x... Additional arguments passed to templatable values.
  /// @details Encodes a NEC frame or repeat code into `dst`.
  /// @note This is a dummy implementation used for interface compatibility only.
  /// This method is not used by the `NECAction` class during transmission.
  ///
  /// @warning `NECCodeType::NEC1_FRAME_WITH_REPEATS` and `NECCodeType::NEC2_FRAME_WITH_REPEATS`
  /// must not be used with this method. Only `NECCodeType::FRAME` and `NECCodeType::REPEAT`
  /// are valid for standalone encoding.
  void encode(RemoteTransmitData *dst, Ts... x) override {
    NECData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    data.repeats = this->send_times_.value_or(x..., 0);
    data.type = this->type_.value(x...);
    NECProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
