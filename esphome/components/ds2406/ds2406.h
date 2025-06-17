#pragma once

#include "esphome/core/component.h"
#include "esphome/components/one_wire/one_wire.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome {
namespace ds2406 {

class Ds2406 : public PollingComponent, public one_wire::OneWireDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(channel_1)
  SUB_BINARY_SENSOR(channel_2)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(channel_1)
  SUB_SWITCH(channel_2)
#endif
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void write_state(uint8_t channel, bool state);
};

}  // namespace ds2406
}  // namespace esphome
