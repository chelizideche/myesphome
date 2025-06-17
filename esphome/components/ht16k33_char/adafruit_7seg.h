#pragma once

#include "ht16k33_char.h"

namespace esphome {
namespace ht16k33_char {

class Adafruit7Seg : public HT16k33CharComponent {
 public:
  uint8_t send_to_display(i2c::I2CDevice *display, uint8_t position) override;
};

class Adafruit7SegFlip : public HT16k33CharComponent {
 public:
  uint8_t send_to_display(i2c::I2CDevice *display, uint8_t position) override;
};

}  // namespace ht16k33_char
}  // namespace esphome
