#pragma once
#include "ht16k33_char.h"

namespace esphome {
namespace ht16k33_char {

class Sparkfun14Seg : public HT16k33CharComponent {
 public:
  uint8_t send_to_display(i2c::I2CDevice *display, uint8_t position) override;

 protected:
  void write_to_buffer_(uint16_t char_to_write, uint8_t char_position);
};

class Sparkfun14SegFlip : public HT16k33CharComponent {
 public:
  uint8_t send_to_display(i2c::I2CDevice *display, uint8_t position) override;

 protected:
  void write_to_buffer_(uint16_t char_to_write, uint8_t char_position);
};

}  // namespace ht16k33_char
}  // namespace esphome
