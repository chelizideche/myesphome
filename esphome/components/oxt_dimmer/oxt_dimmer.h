#pragma once

/*
  Copyright © 2023
*/

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"

namespace esphome {
namespace oxt_dimmer {

class OxtController;

/**
 * OxtDimmerChannel inherits from light::LightOutput and provides "light"
 * functionality towards front-end, ESPHome, HASS...
 */
class OxtDimmerChannel : public light::LightOutput, public Component {
 public:
  // Component overrides
  void dump_config() override;

  // LightOutput overrides
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { light_state_ = state; }
  void write_state(light::LightState *state) override;

  // Own methods
  uint8_t brightness() { return brightness_; }

  void set_min_value(const uint8_t min_value) { min_value_ = min_value; }
  void set_max_value(const uint8_t max_value) { max_value_ = max_value; }
  void set_controller(OxtController *control) { controller_ = control; }

 protected:
  OxtController *controller_{nullptr};

  // light implementation
  uint8_t min_value_{50};
  uint8_t max_value_{255};
  uint8_t brightness_{0};
  light::LightState *light_state_{nullptr};

  void short_press_();
  void periodic_long_press_();
};

/**
 * OxtController class takes care of communication with dimming MCU (back-end)
 * and polling external switch(es) using GPIO input pins
 */
class OxtController : public uart::UARTDevice, public Component {
  friend class OxtDimmerChannel;

 public:
  static constexpr size_t MAX_CHANNELS = 2;

  // Component methods
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  // Own methods
  void add_channel(uint8_t index, OxtDimmerChannel *channel) {
    channels_[index] = channel;
    channel->set_controller(this);
  }

 protected:
  void send_to_mcu_(const OxtDimmerChannel *channel);

 private:
  OxtDimmerChannel *channels_[MAX_CHANNELS]{nullptr};
};

}  // namespace oxt_dimmer
}  // namespace esphome
