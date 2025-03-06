#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp4151 {

// MCP4151
class MCP4151 : public Component, 
                public output::FloatOutput,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_10MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;

 protected:
  uint8_t last_value_{0};
  void send_command(uint8_t value);
};

}  // namespace mcp4151
}  // namespace esphome