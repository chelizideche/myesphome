#pragma once
#include "mipi_spi_base.h"

namespace esphome {
namespace mipi_spi {

/**
 * Templated MipiSpiBuffer class for MIPI SPI displays.
 * This class is designed to copy data from the buffer to the display, so must be paraameterized by the pixel mode of
 * the buffer, the pixel mode of the display, the bus type and the display rotation.
 *
 * @tparam BUFFERTYPE The type of the buffer pixels, e.g. uint8_t or uint16_t
 * @tparam BUFFERPIXEL Color depth of the buffer
 * @tparam DISPLAYPIXEL Color depth of the display
 * @tparam BUS_TYPE The type of the interface bus (single, quad, octal)
 * @tparam ROTATION The rotation of the display
 * @tparam WIDTH Width of the display in pixels
 * @tparam HEIGHT Height of the display in pixels
 * @tparam OFFSET_WIDTH The x-offset of the display in pixels
 * @tparam OFFSET_HEIGHT The y-offset of the display in pixels
 * @tparam FRACTION The fraction of the display size to use for the buffer (e.g. 4 means a 1/4 buffer)
 */
template<typename BUFFERTYPE, PixelMode BUFFERPIXEL, PixelMode DISPLAYPIXEL, BusType BUS_TYPE, int WIDTH, int HEIGHT,
         int OFFSET_WIDTH, int OFFSET_HEIGHT, display::DisplayRotation ROTATION, int FRACTION>
class MipiSpiBuffer : public MipiSpi<WIDTH, HEIGHT, OFFSET_WIDTH, OFFSET_HEIGHT, BUS_TYPE, FRACTION> {
 public:
  MipiSpiBuffer() {
    // set color_depth for others to use
    switch (BUFFERPIXEL) {
      case PIXEL_MODE_8:
        this->color_depth_ = display::COLOR_BITNESS_332;
        break;
      case PIXEL_MODE_16:
        this->color_depth_ = display::COLOR_BITNESS_565;
        break;
      default:
        break;
    }
  }

  // Allocate a buffer if required.
  bool buffer_setup_() override {
    if (this->buffer_size_ != 0) {
      RAMAllocator<BUFFERTYPE> allocator{};
      this->buffer_ = allocator.allocate(this->buffer_size_ / sizeof(BUFFERTYPE));
      if (this->buffer_ == nullptr) {
        this->mark_failed("Buffer allocation failed");
        return false;
      }
    }
    return true;
  }

  int get_width() override {
    if constexpr (ROTATION == display::DISPLAY_ROTATION_90_DEGREES || ROTATION == display::DISPLAY_ROTATION_270_DEGREES)
      return HEIGHT;
    return WIDTH;
  }

  int get_height() override {
    if constexpr (ROTATION == display::DISPLAY_ROTATION_90_DEGREES || ROTATION == display::DISPLAY_ROTATION_270_DEGREES)
      return WIDTH;
    return HEIGHT;
  }

 protected:
  size_t get_buffer_bits_() const override {
    return sizeof(BUFFERTYPE) * 8;  // return the number of bits in a pixel
  }

  size_t get_display_bits_() const override { return DISPLAYPIXEL * 8; }

  size_t get_bus_width_() const override {
    return BUS_TYPE;  // return the number of bits in a bus transfer
  }

  // functions to convert from one pixel format to another

  template<PixelMode B = BUFFERPIXEL, PixelMode D = DISPLAYPIXEL>
  static std::enable_if_t<(B == PIXEL_MODE_8 && D == PIXEL_MODE_16), void> convert_pixel_(const uint8_t *&from,
                                                                                          uint16_t *&to) {
    auto color_val = *from++;
    *to++ = (color_val & 0xE0) | ((color_val & 0x1C) >> 2);
    *to++ = (color_val & 0x3) << 3;
  }

  template<PixelMode B = BUFFERPIXEL, PixelMode D = DISPLAYPIXEL>
  static std::enable_if_t<(B == PIXEL_MODE_16 && D == PIXEL_MODE_18), void> convert_pixel_(const uint16_t *&from,
                                                                                           uint8_t *&to) {
    uint16_t color_val = *from++;
    // deal with byte swapping
    *to++ = color_val << 3;                                           // blue
    *to++ = ((color_val & 0x7) << 5) | ((color_val & 0xE000) >> 11);  // Green
    *to++ = color_val >> 11 << 3;
  }

  template<PixelMode B = BUFFERPIXEL, PixelMode D = DISPLAYPIXEL>
  static std::enable_if_t<(B == PIXEL_MODE_8 && D == PIXEL_MODE_18), void> convert_pixel_(const uint8_t *&from,
                                                                                          uint8_t *&to) {
    auto color_val = *from++;
    *to++ = (color_val & 0xE0);       // Blue
    *to++ = (color_val & 0x1C) << 3;  // Green
    *to++ = color_val << 5;           // Red
  }

  // methods to write display data, given start ptr, width, height and padding
  // the pointer already has the x-offset and y-offset applied, so it points to the first pixel
  // width and pad are in bytes, height in rows

  template<BusType M = BUS_TYPE>
  std::enable_if_t<M == BUS_TYPE_QUAD, void> write_display_data_(const uint8_t *ptr, size_t w, size_t h, size_t pad) {
    this->enable();
    if (pad == 0) {
      this->write_cmd_addr_data(8, 0x32, 24, WDATA << 8, ptr, w * h, 4);
    } else {
      this->write_cmd_addr_data(8, 0x32, 24, WDATA << 8, nullptr, 0, 4);
      for (int y = 0; y != h; y++) {
        this->write_cmd_addr_data(0, 0, 0, 0, reinterpret_cast<const uint8_t *>(ptr), w, 4);
        ptr += w + pad;
      }
    }
    this->disable();
  }

  template<BusType M = BUS_TYPE>
  std::enable_if_t<M == BUS_TYPE_OCTAL, void> write_display_data_(const uint8_t *ptr, size_t w, size_t h, size_t pad) {
    this->write_command_(WDATA);
    this->enable();
    if (pad == 0) {
      this->write_cmd_addr_data(0, 0, 0, 0, reinterpret_cast<const uint8_t *>(ptr), w * h, 8);
    } else {
      for (int y = 0; y != h; y++) {
        this->write_cmd_addr_data(0, 0, 0, 0, reinterpret_cast<const uint8_t *>(ptr), w, 8);
        ptr += w + pad;
      }
    }
    this->disable();
  }

  /**
   * Writes a buffer to the display.
   * @param w Width of each line in bytes
   * @param h Height of the buffer in rows
   * @param pad Padding in bytes after each line
   */
  template<BusType M = BUS_TYPE>
  std::enable_if_t<M == BUS_TYPE_SINGLE || M == BUS_TYPE_SINGLE_16, void> write_display_data_(const uint8_t *ptr,
                                                                                              size_t w, size_t h,
                                                                                              size_t pad) {
    this->write_command_(WDATA);
    ESP_LOGV(TAG, "Write display data: w=%zu, h=%zu, pad=%zu, bytes=%zu", w, h, pad, w * h * sizeof(BUFFERTYPE));
    this->enable();
    if (pad == 0) {
      this->write_array(ptr, w * h);
    } else {
      for (int y = 0; y != h; y++) {
        this->write_array(ptr, w);
        ptr += w + pad;
      }
    }
    this->disable();
  }

  /**
   * Writes a buffer to the display.
   *
   */
  void write_to_display_(int x_start, int y_start, int w, int h, const void *ptr, int x_offset, int y_offset,
                         int x_pad) override {
    if (ptr == nullptr)
      ptr = this->buffer_;
    this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
    const auto *ptr_cast = static_cast<const BUFFERTYPE *>(ptr) + y_offset * (x_offset + w + x_pad) + x_offset;
    if constexpr (BUFFERPIXEL == DISPLAYPIXEL) {
      this->write_display_data_(reinterpret_cast<const uint8_t *>(ptr_cast), w * sizeof(BUFFERTYPE), h,
                                x_pad * sizeof(BUFFERTYPE));
    } else {
      uint8_t dbuffer[6 * 4 * WIDTH];
      uint8_t *dptr = dbuffer;
      for (size_t y = 0; y != h; y++) {
        for (size_t x = 0; x != w; x++) {
          convert_pixel_<BUFFERPIXEL, DISPLAYPIXEL>(ptr_cast, dptr);
          if (dptr == dbuffer + sizeof(dbuffer)) {
            this->write_display_data_(dbuffer, sizeof(dbuffer), 1, 0);
            dptr = dbuffer;
          }
        }
      }
      if (dptr != dbuffer) {
        this->write_display_data_(dbuffer, dptr - dbuffer, 1, 0);
      }
    }
  }

  // Rotate the coordinates for the display rotation

  void rotate_coordinates_(int &x, int &y) const {
    if constexpr (ROTATION == display::DISPLAY_ROTATION_180_DEGREES) {
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
    } else if constexpr (ROTATION == display::DISPLAY_ROTATION_90_DEGREES) {
      std::swap(x, y);
      x = WIDTH - x - 1;
    } else if constexpr (ROTATION == display::DISPLAY_ROTATION_270_DEGREES) {
      std::swap(x, y);
      y = HEIGHT - y - 1;
    }
  }

  BUFFERTYPE convert_color_(Color &color) const {
    if constexpr (BUFFERPIXEL == PIXEL_MODE_8) {
      return color.red & 0xE0 | (color.g & 0xE0) >> 3 | color.b >> 6;
    } else if constexpr (BUFFERPIXEL == PIXEL_MODE_16) {
      uint16_t new_color = color.r >> 3 << 11 | color.g >> 2 << 5 | color.b >> 3;
      return (new_color << 8) | (new_color >> 8);  // Swap bytes for big-endian
    }
    return static_cast<BUFFERTYPE>(0);
  }

  BUFFERTYPE *buffer_{nullptr};

 public:
  // Drawing operations

  void draw_pixel_at(int x, int y, Color color) override {
    if (this->buffer_ == nullptr)
      return;
    rotate_coordinates_(x, y);
    if (x < 0 || x >= WIDTH || y < this->start_line_ || y >= this->end_line_)
      return;
    this->buffer_[(y - this->start_line_) * WIDTH + x] = convert_color_(color);
    if (x < this->x_low_) {
      this->x_low_ = x;
    }
    if (x > this->x_high_) {
      this->x_high_ = x;
    }
    if (y < this->y_low_) {
      this->y_low_ = y;
    }
    if (y > this->y_high_) {
      this->y_high_ = y;
    }
  }

  void fill(Color color) override {
    this->x_low_ = 0;
    this->y_low_ = this->start_line_;
    this->x_high_ = WIDTH - 1;
    this->y_high_ = this->end_line_ - 1;
    std::fill_n(this->buffer_, HEIGHT * WIDTH / FRACTION, convert_color_(color));
  }
};

}  // namespace mipi_spi
}  // namespace esphome
