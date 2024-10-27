#pragma once

#include "esphome/core/log.h"
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"


namespace esphome {
namespace kp18058 {

class kp18058 : public Component {
  public:
      kp18058() : data_pin_(nullptr),
                  clock_pin_(nullptr),
                  max_cw_current_(0),
                  max_rgb_current_(0),
                  channels{nullptr} {}

      void setup() override;
      void dump_config() override;
      void set_data_pin(InternalGPIOPin *data_pin) { data_pin_ = data_pin; }
      void set_clock_pin(InternalGPIOPin *clock_pin) { clock_pin_ = clock_pin; }
      void set_output_channel(uint8_t channel, class kp18058_output* output) { channels[channel-1] = output; }
      void set_cw_current(uint8_t max_cw_current) { max_cw_current_ = max_cw_current; }
      void set_rgb_current(uint8_t max_rgb_current) { max_rgb_current_ = max_rgb_current; }
      void program_led_driver();

  protected:
      InternalGPIOPin *data_pin_;
      InternalGPIOPin *clock_pin_;
      class kp18058_output *channels[5];
      uint8_t max_cw_current_;
      uint8_t max_rgb_current_;

  private:
      bool Soft_I2C_WriteByte(uint8_t value);
      bool Soft_I2C_Start(uint8_t addr);
      void Soft_I2C_Stop();
};


class kp18058_output : public output::FloatOutput {
  public:
      kp18058_output() : parent_(nullptr), value_(0) {}
      void set_parent(kp18058 *parent) { parent_ = parent; }
      uint16_t get_value() { return this->value_; }

  protected:
      void write_state(float state) override
      {
          if (state >= 1) state = 1;
          else if (state <= 0) state = 0;
          value_ = static_cast<uint16_t>(roundf(state * 1023));
          this->parent_->program_led_driver();
      }

      kp18058 *parent_;
      uint16_t value_;

};


}  // namespace kp18058
}  // namespace esphome

