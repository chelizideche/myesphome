#pragma once

/*
  lanbon_l8_hd.h - Lanbon L8-HD Dimmer support for ESPHome

  Copyright © 2025 Janick Bergeron
  Copyright © 2021 Anatoly Savchenkov
  Copyright © 2020 Jeff Rescignano

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  and associated documentation files (the “Software”), to deal in the Software without
  restriction, including without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  -----

  If modifying this file, in addition to the license above, please ensure to include links back to the original code:
  https://jeffresc.dev/blog/2020-10-10
  https://github.com/JeffResc/Sonoff-D1-Dimmer
  https://github.com/arendst/Tasmota/blob/2d4a6a29ebc7153dbe2717e3615574ac1c84ba1d/tasmota/xdrv_37_sonoff_d1.ino#L119-L131

  -----
*/

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace lanbon_l8_hd {

class LocalDimmerOutput : public light::LightOutput, public uart::UARTDevice, public Component {
 public:
  // LightOutput methods
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { this->light_state_ = state; }
  void write_state(light::LightState *state) override;

  // Component methods
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void set_min_value(const uint8_t min_value) { this->min_value_ = min_value; }
  void set_max_value(const uint8_t max_value) { this->max_value_ = max_value; }
  void set_enable(output::BinaryOutput *enable) { this->power_relay_ = enable; }

 protected:
  output::BinaryOutput *power_relay_;
  bool started_{false};
  uint8_t min_value_{0};
  uint8_t max_value_{100};
  bool last_binary_{false};
  uint8_t last_brightness_{0};
  light::LightState *light_state_{nullptr};

  void start_dimmer_();
  void control_dimmer_(bool binary, uint8_t brightness);
  void write_command_(uint8_t *cmd, size_t len);
};

}  // namespace lanbon_l8_hd
}  // namespace esphome
