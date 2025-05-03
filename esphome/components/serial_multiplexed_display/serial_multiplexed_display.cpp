#include "serial_multiplexed_display.h"
#include "sevenseg.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include "driver/timer.h"
#include "esp32-hal-timer.h"
#include "esp32/clk.h"

/*
---------- WARNING -------------
Because this version of the Arduino framework does not allow you to pass a callback arguement, it's impossible to pass
the instance reference into the interrupt handler. The lower level IDF API does allow this. However, we are jumping
down another level of abstraction and potentially breaking how the Arduino API expects to be interfaced with.

The tradeoff here is you can have multiple serial_multiplexed_display components instantiated, and they
will work as expected, because the interrupt handler will call the correct instance for each one. However, we do have
to redefine the hw_timer_t struct in order to make this work.
*/

typedef struct hw_timer_s {
  uint8_t group;
  uint8_t num;
} hw_timer_t;

static bool IRAM_ATTR HOT timer_intr(void *arg) {
  esphome::serial_multiplexed_display::SerialMultiplexedDisplay *inst =
      (esphome::serial_multiplexed_display::SerialMultiplexedDisplay *) arg;

  inst->timer_interrupt();

  return false;
}

namespace esphome {
namespace serial_multiplexed_display {

static const uint8_t SERIAL_MULTIPLEXED_DISPLAY_UNKNOWN_CHAR = 0b11111111;

static const char *const TAG = "display.serial_multiplexed_display";

float SerialMultiplexedDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }

void SerialMultiplexedDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "Serial Multiplexed Display Config:");
  if (model_ == SerialDeviceModel::SN74HC595) {
    ESP_LOGCONFIG(TAG, "  Model: SN74HC595");
  }
  ESP_LOGCONFIG(TAG, "  Length: %d", this->length_);
  LOG_PIN("  CLK Pin: ", this->_clk_pin);
  LOG_PIN("  DATA Pin: ", this->_data_pin);
  if (this->_latch_pin != nullptr) {
    LOG_PIN("  LATCH Pin: ", this->_latch_pin);
  }
  if (this->is_common_cathode == true) {
    ESP_LOGCONFIG(TAG, "  Common Cathode: True");
  } else {
    ESP_LOGCONFIG(TAG, "  Common Cathode: False");
  }
  if (this->is_reversed == true) {
    ESP_LOGCONFIG(TAG, "  Display Reversed: True");
  } else {
    ESP_LOGCONFIG(TAG, "  Display Reversed: False");
  }
}

void SerialMultiplexedDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up....");
  _data_pin->setup();
  _clk_pin->setup();
  if (this->_latch_pin != nullptr) {
    this->_latch_pin->setup();
  }

  // calculate to get around 16ms for the full display based on length. (around 17ms for the entire display)
  //  timer clk = 80Mhz prescaled by 2 = 40Mhz clock.
  uint32_t per_digit_time = 17000 / length_;
  uint64_t timer_match_time = ((uint64_t) per_digit_time * 40000000) / 1000000;
  ESP_LOGCONFIG(TAG, "Calculated display parameters: %d ms per digit, interrupt every %llu cycles", per_digit_time,
                timer_match_time);

  timer = timerBegin(0, 2, true);
  ESP_LOGCONFIG(TAG, "GRP: %d, NUM: %d", timer->group, timer->num);
  timer_group_t grp = static_cast<timer_group_t>(timer->group);
  timer_idx_t num = static_cast<timer_idx_t>(timer->num);
  timer_isr_callback_add(grp, num, &timer_intr, this, 0);
  timerAlarmWrite(timer, timer_match_time, true);
  timerAlarmEnable(timer);
}

void IRAM_ATTR HOT SerialMultiplexedDisplay::timer_interrupt() {
  // push data to the display, as quickly as possible.

  if (!can_update) {
    return;
  }

  if (current_segment >= length_) {
    current_segment = 0;
  }

  if (model_ == SerialDeviceModel::SN74HC595) {
    uint8_t segment_select = 1 << current_segment;

    _latch_pin->digital_write(LOW);
    if (is_common_cathode) {
      serial_shift_out(segment_select, 8, false);
      serial_shift_out(~(buffer_[current_segment]), 8, false);
    } else {
      serial_shift_out(~(segment_select), 8, false);
      serial_shift_out(buffer_[current_segment], 8, false);
    }
    _latch_pin->digital_write(HIGH);
  }

  if (model_ == SerialDeviceModel::CT1642) {
    switch (current_segment) {
      case 0: {
        serial_shift_out(0x7f, 4, true);
        break;
      }
      case 1: {
        serial_shift_out(0xbf, 4, true);
        break;
      }
      case 2: {
        serial_shift_out(0xdf, 4, true);
        break;
      }
      case 3: {
        serial_shift_out(0xef, 4, true);
        break;
      }
    }
    serial_shift_out(0, 6, true);
    serial_shift_out(buffer_[current_segment], 8, true);
    _data_pin->digital_write(LOW);
    _data_pin->digital_write(HIGH);
    _clk_pin->digital_write(LOW);
    _data_pin->digital_write(LOW);
    _data_pin->digital_write(HIGH);
  }

  current_segment++;
}

void IRAM_ATTR HOT SerialMultiplexedDisplay::serial_shift_out(uint8_t val, uint8_t num_bits, bool polarity) {
  for (int i = 0; i < num_bits; i++) {
    this->_data_pin->digital_write(val & (1 << (7 - i)));
    this->_clk_pin->digital_write(true);
    this->_clk_pin->digital_write(false);
  }
}

void SerialMultiplexedDisplay::update() {
  if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  }

  this->display();
}

void SerialMultiplexedDisplay::display() {
  can_update = false;
  memcpy(buffer_, back_buffer_, 4);
  can_update = true;
}

uint8_t SerialMultiplexedDisplay::print(uint8_t pos, const char *str) {
  uint8_t start_pos = pos;

  for (; *str != '\0'; str++) {
    uint8_t disp_data = SERIAL_MULTIPLEXED_DISPLAY_UNKNOWN_CHAR;

    if (*str >= ' ' && *str <= '~') {
      disp_data = progmem_read_byte(&serial_multiplexed_display_translation::SEVEN_SEG[*str - 32]);
    } else if (disp_data == SERIAL_MULTIPLEXED_DISPLAY_UNKNOWN_CHAR) {
      ESP_LOGW(TAG, "Encountered character '%c' with no seven segment representation while translating string!", *str);
    }

    if (pos >= (length_)) {
      ESP_LOGW(TAG, "Requsted display position is past display length.");
      return 0;
    }

    if (is_reversed) {
      back_buffer_[(length_ - 1) - pos] = disp_data;
    } else {
      back_buffer_[pos] = disp_data;
    }

    pos++;
  }
  return pos - start_pos;
}

uint8_t SerialMultiplexedDisplay::print(const char *str) { return this->print(0, str); }

uint8_t SerialMultiplexedDisplay::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}

uint8_t SerialMultiplexedDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(buffer);
  return 0;
}

}  // namespace serial_multiplexed_display
}  // namespace esphome
