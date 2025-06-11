#pragma once
#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_cache.h"
#include "ili9xxx_defines.h"
#include "ili9xxx_init.h"
#include <cmath>

namespace esphome {
namespace i8080 {

static const char *const TAG = "i8080";

enum ColorDepth {
  BITS_8 = 0x08,
  BITS_16 = 0x10,
};

class I8080Display : public display::DisplayBuffer {
 public:
  I8080Display() = default;
  I8080Display(int16_t width, int16_t height, uint8_t cmd_bits, uint8_t param_bits)
      : width_{width}, height_{height}, cmd_bits_{cmd_bits}, param_bits_{param_bits} {}
  void set_dc_pin(InternalGPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_cs_pin(InternalGPIOPin *cs_pin) { this->cs_pin_ = cs_pin; }
  void set_rd_pin(InternalGPIOPin *rd_pin) { this->rd_pin_ = rd_pin; }
  void set_wr_pin(InternalGPIOPin *wr_pin) { this->wr_pin_ = wr_pin; }
  void set_reset_pin(InternalGPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_data_pins(std::vector<InternalGPIOPin *> data_pins) { this->data_pins_ = std::move(data_pins); };
  void set_pclk_hz(float pclk_freq) { pclk_hz_ = std::floor(pclk_freq); }
  void set_dimensions(int16_t width, int16_t height) {
    this->height_ = height;
    this->width_ = width;
  }
  void set_offsets(int16_t offset_x, int16_t offset_y) {
    this->offset_x_ = offset_x;
    this->offset_y_ = offset_y;
  }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  float get_setup_priority() const { return setup_priority::HARDWARE; };
  void set_invert_colors(bool invert) { this->invertcolors_ = invert; }
  void set_buffer_color_depth(ColorDepth mode) { this->buffer_color_mode_ = mode; }
  void set_color_order(display::ColorOrder color_order) { this->color_order_ = color_order; }
  void set_swap_xy(bool swap_xy) { this->swap_xy_ = swap_xy; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }

  void update() override;
  void fill(Color color) override;
  void dump_config() override;
  void setup() override;
  void on_shutdown() override;
  static bool color_trans_done_handler(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                       void *user_ctx);

 protected:
  esp_lcd_i80_bus_handle_t i80_bus_handle_{NULL};
  esp_lcd_i80_bus_config_t i80_bus_config_{};
  esp_lcd_panel_io_handle_t i80_io_handle_{NULL};
  esp_lcd_panel_io_i80_config_t i80_io_config_{};
  esp_lcd_panel_handle_t i80_panel_handle_{NULL};
  esp_lcd_panel_dev_config_t i80_panel_config_{};
  InternalGPIOPin *dc_pin_{nullptr};
  InternalGPIOPin *cs_pin_{nullptr};
  InternalGPIOPin *rd_pin_{nullptr};
  InternalGPIOPin *wr_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
  std::vector<InternalGPIOPin *> data_pins_;
  uint32_t pclk_hz_{4000000};
  uint8_t cmd_bits_{8};
  uint8_t param_bits_{8};
  int16_t width_{0};   ///< Display width as modified by current rotation
  int16_t height_{0};  ///< Display height as modified by current rotation
  int16_t offset_x_{0};
  int16_t offset_y_{0};
  bool invertcolors_{false};
  display::ColorOrder color_order_{display::COLOR_ORDER_RGB};
  ColorDepth buffer_color_mode_{BITS_16};
  bool swap_xy_{false};
  bool mirror_x_{false};
  bool mirror_y_{false};
  bool need_update_{false};
  bool prossing_update_{false};
  bool needs_drawing_{false};
  volatile bool dma_in_process_{false};

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  virtual bool init_chipset_();
  void alloc_buffer_();
  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  // uint8_t *buf_;
  bool check_buffer_();
};

//-----------   ST7789V display --------------
class I8080ST7789V : public I8080Display {
 public:
  I8080ST7789V() : I8080Display(240, 320, 8, 8) {}

 protected:
  bool init_chipset_() override;
};

//-----------   NT35510 display --------------
class I8080NT35510 : public I8080Display {
 public:
  I8080NT35510() : I8080Display(240, 320, 8, 8) {}

 protected:
  bool init_chipset_() override;
};

}  // namespace i8080
}  // namespace esphome
