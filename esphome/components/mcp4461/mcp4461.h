#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp4461 {

/// @brief Struct representing current wiper details/state
/// Default wiper state is 0x80h (128 int)
struct WiperState {
  bool enabled = true;
  uint16_t state = 0;
  optional<float> initial_value;
  bool terminal_a = true;
  bool terminal_b = true;
  bool terminal_w = true;
  bool terminal_hw = true;
  bool wiper_lock_active = false;
  bool update_level = false;
  bool update_terminal = false;
};

/// @brief Class containing i2c commands supported by the device
enum class Mcp4461Commands : uint8_t { WRITE = 0x00, INCREMENT = 0x04, DECREMENT = 0x08, READ = 0x0C };

/// @brief Class containing register addresses of the device
enum class Mcp4461Addresses : uint8_t {
  MCP4461_VW0 = 0x00,
  MCP4461_VW1 = 0x10,
  MCP4461_VW2 = 0x60,
  MCP4461_VW3 = 0x70,
  MCP4461_STATUS = 0x50,
  MCP4461_TCON0 = 0x40,
  MCP4461_TCON1 = 0xA0,
  MCP4461_EEPROM_1 = 0xB0
};

// @brief Class to enumerate allowed channels/wipers
enum class Mcp4461WiperIdx : uint8_t {
  MCP4461_WIPER_0 = 0,
  MCP4461_WIPER_1 = 1,
  MCP4461_WIPER_2 = 2,
  MCP4461_WIPER_3 = 3,
  MCP4461_WIPER_4 = 4,
  MCP4461_WIPER_5 = 5,
  MCP4461_WIPER_6 = 6,
  MCP4461_WIPER_7 = 7
};

/// @brief Class containing valid EEProm user data locations
enum class Mcp4461EepromLocation : uint8_t {
  MCP4461_EEPROM_0 = 0,
  MCP4461_EEPROM_1 = 1,
  MCP4461_EEPROM_2 = 2,
  MCP4461_EEPROM_3 = 3,
  MCP4461_EEPROM_4 = 4
};

/// @brief Class containing valid terminal register params for terminal functions
enum class Mcp4461TerminalIdx : uint8_t { MCP4461_TERMINAL_0 = 0, MCP4461_TERMINAL_1 = 1 };

class Mcp4461Wiper;

// Mcp4461Component
class Mcp4461Component : public Component, public i2c::I2CDevice {
 public:
  Mcp4461Component(bool disable_wiper_0, bool disable_wiper_1, bool disable_wiper_2, bool disable_wiper_3)
      : wiper_0_disabled_(disable_wiper_0),
        wiper_1_disabled_(disable_wiper_1),
        wiper_2_disabled_(disable_wiper_2),
        wiper_3_disabled_(disable_wiper_3) {
    this->reg_[0].enabled = !this->wiper_0_disabled_;
    this->reg_[1].enabled = !this->wiper_1_disabled_;
    this->reg_[2].enabled = !this->wiper_2_disabled_;
    this->reg_[3].enabled = !this->wiper_3_disabled_;
  }
  /// @brief perform initialisation of component
  void setup() override;
  /// @brief dump component config on boot
  void dump_config() override;
  /// @brief set hardware priority for component
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  /// @brief get user-data value from eeprom location
  /// @param[in] location The location to fetch data from, valid is uint8_t in range of 0-4
  /// mcp4461 provides 5x 9 bits (=max int of 511) of user-data storage
  /// @return uint16_t - returns the eeprom value stored in given eeprom user-data location
  uint16_t get_eeprom_value(Mcp4461EepromLocation location);
  /// @brief set user-data value from eeprom location
  /// @param[in] location The location to write data to, valid is uint8_t in range of 0-4 for 5x 9 bits of user-data
  /// @return bool - return true on successful write, false on error/warning
  bool set_eeprom_value(Mcp4461EepromLocation location, uint16_t value);
  /// @brief set initial value for wiper
  /// @param[wiper] wiper for which initial_value shall be set
  /// @param[initial_value] the float value in range 0 to 1.0 the wiper shall be set to
  void set_initial_value(Mcp4461WiperIdx wiper, float initial_value);
  /// @brief initialize terminal of wiper with state disabled
  /// @param[wiper] wiper for which terminal shall be initialized disabled
  /// @param[terminal] terminal to disable, one of { 'a', 'b', 'w', 'h' }
  void initialize_terminal_disabled(Mcp4461WiperIdx wiper, char terminal);

  /// @brief available/required status codes
  enum ErrorCode {
    MCP4461_STATUS_OK = 0,               // CMD completed successfully
    MCP4461_FAILED,                      // component failed
    MCP4461_STATUS_I2C_ERROR,            // Unable to communicate with device
    MCP4461_STATUS_REGISTER_INVALID,     // Status register value was invalid
    MCP4461_STATUS_REGISTER_ERROR,       // Error fetching status register
    MCP4461_PROHIBITED_FOR_NONVOLATILE,  //
    MCP4461_VALUE_INVALID,               // Invalid value given for wiper / eeprom
    MCP4461_WRITE_PROTECTED,  // The value was read, but the CRC over the payload (valid and data) does not match
    MCP4461_WIPER_ENABLED,    // The wiper is enabled, discard additional enabling actions
    MCP4461_WIPER_DISABLED,   // The wiper is disabled - all actions for this wiper will be aborted/discarded
    MCP4461_WIPER_LOCKED,     // The wiper is locked using WiperLock-technology - all actions for this wiper will be
                              // aborted/discarded
  } error_code{MCP4461_STATUS_OK};

 protected:
  friend class Mcp4461Wiper;
  /// @brief update write protection status of device
  void update_write_protection_status_();
  /// @brief fetch wiper address for given wiper
  /// @param[wiper] wiper to fetch address for, int in range 0-7
  /// @return wiper address from Mcp4461Addresses
  uint8_t get_wiper_address_(uint8_t wiper);
  /// @brief internal i2c function to read given wiper value
  /// @return uint16_t value in range 0-256 representing current wiper state/level
  uint16_t read_wiper_level_(uint8_t wiper);
  /// @brief fetch device status register values
  /// @return uint8_t status register value - see datasheet for bit values
  uint8_t get_status_register_();
  /// @brief read current level/state of given wiper with validation checks
  /// @return uint16_t value in range 0-256 representing current wiper state/level
  uint16_t get_wiper_level_(Mcp4461WiperIdx wiper);
  /// @brief set level/state of given wiper
  /// @return bool - true on success, false on error/warning
  bool set_wiper_level_(Mcp4461WiperIdx wiper, uint16_t value);
  /// @brief update current level/state of given wiper
  /// @return bool - true on success, false on error/warning
  bool update_wiper_level_(Mcp4461WiperIdx wiper);
  /// @brief enable given wiper
  /// @param[in] wiper The wiper to enable
  void enable_wiper_(Mcp4461WiperIdx wiper);
  /// @brief disable given wiper
  /// @param[in] wiper The wiper to disable
  void disable_wiper_(Mcp4461WiperIdx wiper);
  /// @brief increase given wiper
  /// @param[wiper] wiper to increase
  /// @return bool - true on success, false on error/warning
  bool increase_wiper_(Mcp4461WiperIdx wiper);
  /// @brief increase given wiper
  /// @param[wiper] wiper to increase
  /// @return bool - true on success, false on error/warning
  bool decrease_wiper_(Mcp4461WiperIdx wiper);
  /// @brief enable terminal of wiper
  /// @param[wiper] desired wiper for which the terminal shall be enabled
  void enable_terminal_(Mcp4461WiperIdx wiper, char terminal);
  /// @brief disable terminal of wiper
  /// @param[wiper] desired wiper for which the terminal shall be disabled
  void disable_terminal_(Mcp4461WiperIdx, char terminal);
  /// @brief check if device is still busy writing to eeprom
  /// @return bool - true if device is currently writing to eeprom
  bool is_writing_();
  /// @brief wait until timeout if device is busy
  /// @param[wait_if_not_ready] set to true to wait until timeout again, if previous write timed out already
  /// @return bool - true if device eeprom still busy, false if rdy for write to nonvolatile wiper/eeprom
  bool is_eeprom_ready_for_writing_(bool wait_if_not_ready);
  /// @brief set wiper level
  /// @param[wiper] wiper for which the new state shall be set
  /// @param[value] the int value in range 0-256 the wiper shall be set to
  void write_wiper_level_(uint8_t wiper, uint16_t value);
  /// @brief internal i2c write function
  /// @return bool - true write successful, false if not
  bool mcp4461_write_(uint8_t addr, uint16_t data, bool nonvolatile = false);
  /// @brief calculate correct terminal register values
  /// @return uint8_t - calculated terminal register value for current internal terminal states
  uint8_t calc_terminal_connector_byte_(Mcp4461TerminalIdx terminal_connector);
  /// @brief internal function to update terminal registers
  void update_terminal_register_(Mcp4461TerminalIdx terminal_connector);
  /// @brief internal function to get terminal register values
  /// /// @return uint8_t - get terminal register value of specified terminal
  uint8_t get_terminal_register_(Mcp4461TerminalIdx terminal_connector);
  /// @brief internal function to set terminal registers
  /// @return bool - true if write successful, false if not
  bool set_terminal_register_(Mcp4461TerminalIdx terminal_connector, uint8_t data);

  WiperState reg_[8];
  void begin_();
  bool last_eeprom_write_timed_out_{false};
  bool write_protected_{false};
  bool wiper_0_disabled_{false};
  bool wiper_1_disabled_{false};
  bool wiper_2_disabled_{false};
  bool wiper_3_disabled_{false};
};
}  // namespace mcp4461
}  // namespace esphome
