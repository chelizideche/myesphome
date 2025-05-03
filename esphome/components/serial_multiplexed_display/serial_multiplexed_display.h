#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <esp32-hal-timer.h>
#endif

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

namespace esphome {
namespace serial_multiplexed_display {

class SerialMultiplexedDisplay;

using serial_multiplexed_display_writer_t = std::function<void(SerialMultiplexedDisplay &)>;

enum SerialDeviceModel { SN74HC595, CT1642 };

class SerialMultiplexedDisplay : public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void display();

  uint8_t print(uint8_t pos, const char *str);
  uint8_t print(const char *str);
  uint8_t printf(uint8_t pos, const char *format, ...);
  uint8_t printf(const char *format, ...);

  float get_setup_priority() const override;

  void set_clk_pin(GPIOPin *pin) { _clk_pin = pin; }
  void set_data_pin(GPIOPin *pin) { _data_pin = pin; }
  void set_latch_pin(GPIOPin *pin) { _latch_pin = pin; }
  void set_model(SerialDeviceModel model) { this->model_ = model; }
  void set_length(uint8_t length) { this->length_ = length; }
  void set_common_cathode(bool common_cathode) { this->is_common_cathode = common_cathode; }
  void set_reversed(bool reversed) { this->is_reversed = reversed; }

  void set_writer(serial_multiplexed_display_writer_t &&writer) { this->writer_ = writer; }

  void IRAM_ATTR HOT timer_interrupt();

 protected:
  GPIOPin *_clk_pin;
  GPIOPin *_data_pin;
  GPIOPin *_latch_pin;

  SerialDeviceModel model_{};
  uint8_t length_;

  bool is_common_cathode = false;
  bool is_reversed = false;

  uint8_t buffer_[4] = {0};
  uint8_t back_buffer_[4] = {0};
  optional<serial_multiplexed_display_writer_t> writer_{};

  bool can_update = true;

 private:
  void serial_shift_out(uint8_t val, uint8_t num_bits, bool polarity);
  uint8_t current_segment = 0;

#ifdef USE_ESP32
  hw_timer_t *timer = nullptr;
#endif
};

}  // namespace serial_multiplexed_display
}  // namespace esphome
