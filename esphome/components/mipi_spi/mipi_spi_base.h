#pragma once

#include <utility>

#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"
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
 * Un-templated base class for MIPI SPI displays.
 * This defines methods and properties that don't depend on the pixel mode or bus type.
 */

class MipiSpi : public display::DisplayBuffer,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  MipiSpi(size_t width, size_t height, int16_t offset_width, int16_t offset_height)
      : width_(width), height_(height), offset_width_(offset_width), offset_height_(offset_height) {}
  void set_model(const char *model) { this->model_ = model; }
  void update() override;
  void setup() override;

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

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void dump_config() override;

  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  bool can_proceed() override { return this->setup_complete_; }
  void set_init_sequence(const std::vector<uint8_t> &sequence) { this->init_sequence_ = sequence; }
  void set_draw_rounding(unsigned rounding) { this->draw_rounding_ = rounding; }

 protected:
  /* METHODS */
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
  // convenience functions to write commands with or without data
  void write_command_(uint8_t cmd, uint8_t data) { this->write_command_(cmd, &data, 1); }
  void write_command_(uint8_t cmd) { this->write_command_(cmd, &cmd, 0); }
  // update the display with changed parameters e.g. brightness
  void reset_params_();
  // reset the display, and write the init sequence
  void write_init_sequence_();
  // set the address window for the next write
  void set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

  // virtual functions implemented in templated subclasses

  // Writes a buffer to the display.
  virtual void write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                                 int x_pad) = 0;
  // Writes a command to the display, with the given bytes.
  virtual void write_command_(uint8_t cmd, const uint8_t *bytes, size_t len) = 0;
  // get size in bits of the pixel buffer and display.
  virtual size_t get_buffer_bits_() const = 0;
  virtual size_t get_display_bits_() const = 0;
  virtual size_t get_bus_width_() const = 0;

  /* PROPERTIES */

  // dimensions, set in the constructor
  size_t width_;
  size_t height_;
  int16_t offset_width_;
  int16_t offset_height_;

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
  size_t buffer_bytes_{0};
  uint8_t madctl_{};
};

}  // namespace mipi_spi
}  // namespace esphome
