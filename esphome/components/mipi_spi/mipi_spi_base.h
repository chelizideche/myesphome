#pragma once

#include <utility>

#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"

namespace esphome {
namespace mipi_spi {

constexpr static const char *const TAG = "display.mipi_spi";
static constexpr uint8_t SW_RESET_CMD = 0x01;
static constexpr uint8_t SLEEP_OUT = 0x11;
static constexpr uint8_t NORON = 0x13;
static constexpr uint8_t INVERT_OFF = 0x20;
static constexpr uint8_t INVERT_ON = 0x21;
static constexpr uint8_t ALL_ON = 0x23;
static constexpr uint8_t WRAM = 0x24;
static constexpr uint8_t MIPI = 0x26;
static constexpr uint8_t DISPLAY_ON = 0x29;
static constexpr uint8_t RASET = 0x2B;
static constexpr uint8_t CASET = 0x2A;
static constexpr uint8_t WDATA = 0x2C;
static constexpr uint8_t TEON = 0x35;
static constexpr uint8_t MADCTL_CMD = 0x36;
static constexpr uint8_t PIXFMT = 0x3A;
static constexpr uint8_t BRIGHTNESS = 0x51;
static constexpr uint8_t SWIRE1 = 0x5A;
static constexpr uint8_t SWIRE2 = 0x5B;
static constexpr uint8_t PAGESEL = 0xFE;

static constexpr uint8_t MADCTL_MY = 0x80;     // Bit 7 Bottom to top
static constexpr uint8_t MADCTL_MX = 0x40;     // Bit 6 Right to left
static constexpr uint8_t MADCTL_MV = 0x20;     // Bit 5 Swap axes
static constexpr uint8_t MADCTL_RGB = 0x00;    // Bit 3 Red-Green-Blue pixel order
static constexpr uint8_t MADCTL_BGR = 0x08;    // Bit 3 Blue-Green-Red pixel order
static constexpr uint8_t MADCTL_XFLIP = 0x02;  // Mirror the display horizontally
static constexpr uint8_t MADCTL_YFLIP = 0x01;  // Mirror the display vertically

static const uint8_t DELAY_FLAG = 0xFF;
// store a 16 bit value in a buffer, big endian.
static inline void put16_be(uint8_t *buf, uint16_t value) {
  buf[0] = value >> 8;
  buf[1] = value;
}

// Buffer mode, conveniently also the number of bytes in a pixel
enum PixelMode {
  PIXEL_MODE_8 = 1,
  PIXEL_MODE_16 = 2,
  PIXEL_MODE_18 = 3,
};

enum BusType {
  BUS_TYPE_SINGLE = 1,
  BUS_TYPE_QUAD = 4,
  BUS_TYPE_OCTAL = 8,
  BUS_TYPE_SINGLE_16 = 16,  // Single bit bus, but 16 bits per transfer
};

/**
 * Base class for MIPI SPI displays.
 * This defines methods and properties that don't depend on the pixel mode
 */

template<int WIDTH, int HEIGHT, int OFFSET_WIDTH, int OFFSET_HEIGHT, BusType BUS_TYPE, int FRACTION>
class MipiSpi : public display::Display,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  MipiSpi() {}
  void set_model(const char *model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_invert_colors(bool invert_colors) {
    this->invert_colors_ = invert_colors;
    this->reset_params_();
  }
  void set_brightness(uint8_t brightness) {
    this->brightness_ = brightness;
    this->reset_params_();
  }
  void set_buffer_size(size_t buffer_size) { this->buffer_size_ = buffer_size; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  int get_width_internal() override { return WIDTH; }
  int get_height_internal() override { return HEIGHT; }
  void set_init_sequence(const std::vector<uint8_t> &sequence) { this->init_sequence_ = sequence; }
  void set_draw_rounding(unsigned rounding) { this->draw_rounding_ = rounding; }

 protected:
  /* METHODS */
  // convenience functions to write commands with or without data
  void write_command_(uint8_t cmd, uint8_t data) { this->write_command_(cmd, &data, 1); }
  void write_command_(uint8_t cmd) { this->write_command_(cmd, &cmd, 0); }
  // update the display with changed parameters e.g. brightness
  // reset the display, and write the init sequence
  // set the address window for the next write

  // virtual functions implemented in templated subclasses

  // Writes a buffer to the display.
  virtual void write_to_display_(int x_start, int y_start, int w, int h, const void *ptr, int x_offset, int y_offset,
                                 int x_pad) = 0;
  // get size in bits of the pixel buffer and display.
  virtual size_t get_buffer_bits_() const = 0;
  virtual size_t get_display_bits_() const = 0;
  virtual size_t get_bus_width_() const = 0;
  virtual bool buffer_setup_() = 0;

  void setup() override {
    ESP_LOGCONFIG(TAG, "Running setup");
    this->spi_setup();
    if (this->dc_pin_ != nullptr) {
      this->dc_pin_->setup();
      this->dc_pin_->digital_write(false);
    }
    for (auto *pin : this->enable_pins_) {
      pin->setup();
      pin->digital_write(true);
    }
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->setup();
      this->reset_pin_->digital_write(true);
      delay(5);
      this->reset_pin_->digital_write(false);
      delay(5);
      this->reset_pin_->digital_write(true);
    }

    // need to know when the display is ready for SLPOUT command - will be 120ms after reset
    auto when = millis() + 120;
    delay(10);
    size_t index = 0;
    auto &vec = this->init_sequence_;
    while (index != vec.size()) {
      if (vec.size() - index < 2) {
        ESP_LOGE(TAG, "Malformed init sequence");
        this->mark_failed();
        return;
      }
      uint8_t cmd = vec[index++];
      uint8_t x = vec[index++];
      if (x == DELAY_FLAG) {
        ESP_LOGD(TAG, "Delay %dms", cmd);
        delay(cmd);
      } else {
        uint8_t num_args = x & 0x7F;
        if (vec.size() - index < num_args) {
          ESP_LOGE(TAG, "Malformed init sequence");
          this->mark_failed();
          return;
        }
        auto arg_byte = vec[index];
        switch (cmd) {
          case SLEEP_OUT: {
            // are we ready, boots?
            int duration = when - millis();
            if (duration > 0) {
              ESP_LOGD(TAG, "Sleep %dms", duration);
              delay(duration);
            }
          } break;

          case INVERT_ON:
            this->invert_colors_ = true;
            break;
          case MADCTL_CMD:
            this->madctl_ = arg_byte;
            break;
          case BRIGHTNESS:
            this->brightness_ = arg_byte;
            break;

          default:
            break;
        }
        const auto *ptr = vec.data() + index;
        ESP_LOGD(TAG, "Command %02X, length %d, byte %02X", cmd, num_args, arg_byte);
        this->write_command_(cmd, ptr, num_args);
        index += num_args;
        if (cmd == SLEEP_OUT)
          delay(10);
      }
    }
    this->init_sequence_.clear();
    if (this->buffer_setup_())
      this->setup_complete_ = true;
    ESP_LOGCONFIG(TAG, "MIPI SPI setup complete");
  }

  void update() {
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
    auto now = millis();
#endif
    ESP_LOGV(TAG, "Update called");
    if (!this->setup_complete_ || this->is_failed()) {
      return;
    }
    for (this->start_line_ = 0; this->start_line_ < HEIGHT; this->start_line_ += HEIGHT / FRACTION) {
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
      auto lap = millis();
#endif
      this->end_line_ = this->start_line_ + HEIGHT / FRACTION;
      if (this->auto_clear_enabled_) {
        this->clear();
      }
      if (this->show_test_card_) {
        this->test_card();
      } else if (this->page_ != nullptr) {
        this->page_->get_writer()(*this);
      } else if (this->writer_.has_value()) {
        (*this->writer_)(*this);
      } else {
        return;
      }
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
      ESP_LOGV(TAG, "Drawing from line %d took %dms", this->start_line_, millis() - lap);
      lap = millis();
#endif
      if (this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
        return;
      ESP_LOGV(TAG, "x_low %d, y_low %d, x_high %d, y_high %d", this->x_low_, this->y_low_, this->x_high_,
               this->y_high_);
      // Some chips require that the drawing window be aligned on certain boundaries
      auto dr = this->draw_rounding_;
      this->x_low_ = this->x_low_ / dr * dr;
      this->y_low_ = this->y_low_ / dr * dr;
      this->x_high_ = (this->x_high_ + dr) / dr * dr - 1;
      this->y_high_ = (this->y_high_ + dr) / dr * dr - 1;
      int w = this->x_high_ - this->x_low_ + 1;
      int h = this->y_high_ - this->y_low_ + 1;
      this->write_to_display_(this->x_low_, this->y_low_, w, h, nullptr, this->x_low_, this->y_low_ - this->start_line_,
                              WIDTH - w);
      // invalidate watermarks
      this->x_low_ = WIDTH;
      this->y_low_ = HEIGHT;
      this->x_high_ = 0;
      this->y_high_ = 0;
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
      ESP_LOGV(TAG, "Write to display took %dms", millis() - lap);
      lap = millis();
#endif
    }
    ESP_LOGV(TAG, "Total update took %dms", millis() - now);
  }

  // Writes a command to the display, with the given bytes.
  void write_command_(uint8_t cmd, const uint8_t *bytes, size_t len) {
    ESP_LOGV(TAG, "Command %02X, length %d, bytes %s", cmd, len, format_hex_pretty(bytes, len).c_str());
    if constexpr (BUS_TYPE == BUS_TYPE_QUAD) {
      this->enable();
      this->write_cmd_addr_data(8, 0x02, 24, cmd << 8, bytes, len);
      this->disable();
    } else if constexpr (BUS_TYPE == BUS_TYPE_OCTAL) {
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_cmd_addr_data(0, 0, 0, 0, &cmd, 1, 8);
      this->disable();
      this->dc_pin_->digital_write(true);
      if (len != 0) {
        this->enable();
        this->write_cmd_addr_data(0, 0, 0, 0, bytes, len, 8);
        this->disable();
      }
    } else if constexpr (BUS_TYPE == BUS_TYPE_SINGLE) {
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_byte(cmd);
      this->disable();
      this->dc_pin_->digital_write(true);
      if (len != 0) {
        this->enable();
        this->write_array(bytes, len);
        this->disable();
      }
    } else if constexpr (BUS_TYPE == BUS_TYPE_SINGLE_16) {
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_byte(cmd);
      this->disable();
      this->dc_pin_->digital_write(true);
      for (size_t i = 0; i != len; i++) {
        this->enable();
        this->write_byte(0);
        this->write_byte(bytes[i]);
        this->disable();
      }
    }
  }
  void reset_params_() {
    if (!this->is_ready())
      return;
    this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
    if (this->brightness_.has_value())
      this->write_command_(BRIGHTNESS, this->brightness_.value());
  }

  void write_init_sequence_() {
    size_t index = 0;
    auto &vec = this->init_sequence_;
    while (index != vec.size()) {
      if (vec.size() - index < 2) {
        ESP_LOGE(TAG, "Malformed init sequence");
        this->mark_failed();
        return;
      }
      uint8_t cmd = vec[index++];
      uint8_t x = vec[index++];
      if (x == DELAY_FLAG) {
        ESP_LOGV(TAG, "Delay %dms", cmd);
        delay(cmd);
      } else {
        uint8_t num_args = x & 0x7F;
        if (vec.size() - index < num_args) {
          ESP_LOGE(TAG, "Malformed init sequence");
          this->mark_failed();
          return;
        }
        const auto *ptr = vec.data() + index;
        this->write_command_(cmd, ptr, num_args);
        index += num_args;
      }
    }
    ESP_LOGCONFIG(TAG, "MIPI SPI setup complete");
  }

  void set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    ESP_LOGV(TAG, "Set addr %d/%d, %d/%d", x1, y1, x2, y2);
    uint8_t buf[4];
    x1 += OFFSET_WIDTH;
    x2 += OFFSET_WIDTH;
    y1 += OFFSET_HEIGHT;
    y2 += OFFSET_HEIGHT;
    put16_be(buf, y1);
    put16_be(buf + 2, y2);
    this->write_command_(RASET, buf, sizeof buf);
    put16_be(buf, x1);
    put16_be(buf + 2, x2);
    this->write_command_(CASET, buf, sizeof buf);
  }

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    if (!this->setup_complete_ || this->is_failed())
      return;
    if (w <= 0 || h <= 0)
      return;
    if (bitness != this->color_depth_ || big_endian != (this->bit_order_ == spi::BIT_ORDER_MSB_FIRST)) {
      ESP_LOGE(TAG, "Unsupported color depth or bit order");
      return;
    }
    this->write_to_display_(x_start, y_start, w, h, ptr, x_offset, y_offset, x_pad);
  }

  void dump_config() {
    ESP_LOGCONFIG(TAG,
                  "MIPI_SPI Display\n"
                  "  Model: %s\n"
                  "  Width: %u\n"
                  "  Height: %u",
                  this->model_, WIDTH, HEIGHT);
    if constexpr (OFFSET_WIDTH != 0)
      ESP_LOGCONFIG(TAG, "  Offset width: %u", OFFSET_WIDTH);
    if constexpr (OFFSET_HEIGHT != 0)
      ESP_LOGCONFIG(TAG, "  Offset height: %u", OFFSET_HEIGHT);
    ESP_LOGCONFIG(TAG,
                  "  Swap X/Y: %s\n"
                  "  Mirror X: %s\n"
                  "  Mirror Y: %s\n"
                  "  Rotation: %d°\n"
                  "  Invert colors: %s\n"
                  "  Color order: %s\n"
                  "  Buffer pixels: %d bits\n"
                  "  Display pixels: %d bits\n"
                  "  Buffer fraction: 1/%d\n"
                  "  Buffer bytes: %zu",
                  YESNO(this->madctl_ & MADCTL_MV), YESNO(this->madctl_ & (MADCTL_MX | MADCTL_XFLIP)),
                  YESNO(this->madctl_ & (MADCTL_MY | MADCTL_YFLIP)), this->rotation_, YESNO(this->invert_colors_),
                  this->madctl_ & MADCTL_BGR ? "BGR" : "RGB", this->get_buffer_bits_(), this->get_display_bits_(),
                  FRACTION, this->buffer_size_);
    if (this->brightness_.has_value())
      ESP_LOGCONFIG(TAG, "  Brightness: %u", this->brightness_.value());
    ESP_LOGCONFIG(TAG, "  Draw rounding: %u", this->draw_rounding_);
    LOG_PIN("  CS Pin: ", this->cs_);
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
    LOG_PIN("  DC Pin: ", this->dc_pin_);
    ESP_LOGCONFIG(TAG,
                  "  SPI Mode: %d\n"
                  "  SPI Data rate: %dMHz\n"
                  "  SPI Bus width: %d",
                  this->mode_, static_cast<unsigned>(this->data_rate_ / 1000000), this->get_bus_width_());
  }

  /* PROPERTIES */

  // GPIO pins
  GPIOPin *reset_pin_{nullptr};
  std::vector<GPIOPin *> enable_pins_{};
  GPIOPin *dc_pin_{nullptr};

  // other properties set by configuration
  bool invert_colors_{};
  unsigned draw_rounding_{2};
  optional<uint8_t> brightness_{};
  const char *model_{"Unknown"};
  std::vector<uint8_t> init_sequence_{};
  display::ColorBitness color_depth_{};

  // internally used properties
  // capture bounds of last draw operation
  uint16_t x_low_{1};
  uint16_t y_low_{1};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  bool setup_complete_{};
  size_t buffer_size_{0};  // buffer size in bytes, 0 means no buffer
  uint8_t madctl_{};
  uint16_t start_line_{0};
  uint16_t end_line_{HEIGHT / FRACTION};
};

}  // namespace mipi_spi
}  // namespace esphome
