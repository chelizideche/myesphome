#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#define get_single_bit(value, bit)            (((value) >> (bit)) & 0x01)
#define set_single_bit(value, bit)             ((value) |= (1UL << (bit)))

namespace esphome {
namespace mcp4461 {

struct WiperState {
  bool terminal_a = true;
  bool terminal_b = true;
  bool terminal_w = true;
  bool terminal_hw = true;
  uint16_t state;
  bool enabled = true;
};

enum class MCP4461_DEFAULTS { WIPER_VALUE = 0x80 };
enum class MCP4461_COMMANDS { WRITE = 0x0, INCREMENT = 0x4, DECREMENT = 0x8, READ = 0xC };
enum class MCP4461_ADDRESSES { MCP4461_VW0 = 0x00, MCP4461_VW1 = 0x10, MCP4461_VW2 = 0x60, MCP4461_VW3 = 0x70, MCP4461_STATUS = 0x50, MCP4461_TCON0 = 0x40, MCP4461_TCON1 = 0xA0, MCP4461_EEPROM_1 = 0xB0 };
enum MCP4461WiperIdx {
  MCP4461_WIPER_0 = 0, MCP4461_WIPER_1 = 1, MCP4461_WIPER_2 = 2, MCP4461_WIPER_3 = 3, //volatile wipers 0-3
  MCP4461_WIPER_4 = 4, MCP4461_WIPER_5 = 5, MCP4461_WIPER_6 = 6, MCP4461_WIPER_7 = 7 //non-volatile wipers 0-3 (wiper id -4 = volatile wiper)
};
enum MCP4461EEPRomLocation { MCP4461_EEPROM_0 = 0, MCP4461_EEPROM_1 = 1, MCP4461_EEPROM_2 = 2, MCP4461_EEPROM_3 = 3, MCP4461_EEPROM_4 = 4 };
enum class MCP4461TerminalIdx { MCP4461_TERMINAL_0 = 0, MCP4461_TERMINAL_1 = 1 };

class MCP4461Wiper;

/// MCP4461 component.
class MCP4461Component : public Component, public i2c::I2CDevice {
 public:
   MCP4461Component(bool disable_wiper_0, bool disable_wiper_1, bool disable_wiper_2, bool disable_wiper_3) : wiper_0_enabled_(false), wiper_1_enabled_(false), wiper_2_enabled_(false), wiper_3_enabled_(false) {
     
     this->reg_[0].enabled = this->wiper_0_enabled_;
     this->reg_[1].enabled = this->wiper_1_enabled_;
     this->reg_[2].enabled = this->wiper_2_enabled_;
     this->reg_[3].enabled = this->wiper_3_enabled_;
   }

   void setup() override;
   void dump_config() override;
   float get_setup_priority() const override { return setup_priority::HARDWARE; }
   void loop() override;
   uint16_t get_eeprom_value(MCP4461EEPRomLocation location);
   void set_eeprom_value(MCP4461EEPRomLocation location, uint16_t value);

 protected:
   friend MCP4461Wiper;
   uint8_t get_wiper_address_(uint8_t wiper);
   void update_wiper_state_(uint8_t wiper);
   void write_wiper_level_(uint8_t wiper, uint16_t value);
   void enable_wiper_(uint8_t wiper);
   void disable_wiper_(uint8_t wiper);
   void increase_wiper_(uint8_t wiper);
   void decrease_wiper_(uint8_t wiper);
   void mcp4461_write_(uint8_t addr, uint16_t data);
   uint8_t calc_terminal_connector_byte_(MCP4461TerminalIdx terminal_connector);
   void update_terminal_register_(MCP4461TerminalIdx terminal_connector);
   void enable_terminal_(uint8_t wiper, char terminal);
   void disable_terminal_(uint8_t wiper, char terminal);
   uint8_t get_terminal_register_(MCP4461TerminalIdx terminal_connector);
   void set_terminal_register_(MCP4461TerminalIdx terminal_connector, uint8_t data);
   uint16_t get_status_register_();
   uint16_t get_wiper_level_(uint8_t wiper);
   void set_wiper_level_(uint8_t wiper, uint16_t value);
   bool is_writing_();

 private:
   WiperState reg_[8];
   void begin_();
   bool update_ = false;
   bool wiper_0_enabled_ = true;
   bool wiper_1_enabled_ = true;
   bool wiper_2_enabled_ = true;
   bool wiper_3_enabled_ = true;
};

}
}
