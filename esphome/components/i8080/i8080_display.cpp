#include "i8080_display.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace i8080 {

void I8080Display::dump_config(void) {
  LOG_DISPLAY("", "i8080", this);
  ESP_LOGCONFIG(TAG, "  Width Offset: %u", this->offset_x_);
  ESP_LOGCONFIG(TAG, "  Height Offset: %u", this->offset_y_);
  switch (this->buffer_color_mode_) {
    case BITS_16:
      ESP_LOGCONFIG(TAG, "  Color mode: 16bit");
      break;
    default:
      ESP_LOGCONFIG(TAG, "  Color mode: 8bit 332 mode");
      break;
  }
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  RD Pin: ", this->rd_pin_);
  LOG_PIN("  WR Pin: ", this->wr_pin_);
  for (uint8_t i = 0; i < this->data_pins_.size(); i++)
    ESP_LOGCONFIG(TAG, "  Data Pin %d: %s", i, this->data_pins_[i]->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "  Color order: %s", this->color_order_ == display::COLOR_ORDER_BGR ? "BGR" : "RGB");
  ESP_LOGCONFIG(TAG, "  Swap_xy: %s", YESNO(this->swap_xy_));
  ESP_LOGCONFIG(TAG, "  Mirror_x: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror_y: %s", YESNO(this->mirror_y_));
  ESP_LOGCONFIG(TAG, "  Invert colors: %s", YESNO(this->invertcolors_));

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  => Failed to init Memory: YES!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void I8080Display::setup(void) {
  ESP_LOGD(TAG, "Setting up I8080");
  if (!this->check_buffer_()) {
    this->mark_failed();
    ESP_LOGE(TAG, "Failed to allocate display buffer memory!");
    return;
  }

  // Setup the RD Pin as always high (not used yet).
  // Needed if the display RD pin is hardwired to the ESP32 GPIO.
  if (this->rd_pin_ != nullptr) {
    this->rd_pin_->setup();
    this->rd_pin_->digital_write(true);
  }

  // Setup bus
  this->i80_bus_config_ = {
      .dc_gpio_num = this->dc_pin_->get_pin(),
      .wr_gpio_num = this->wr_pin_->get_pin(),
      .clk_src = LCD_CLK_SRC_PLL160M,
      .bus_width = this->data_pins_.size(),
      .max_transfer_bytes = this->get_buffer_length_(),
      .psram_trans_align = 32,
      .sram_trans_align = 0,
  };
  for (uint8_t i = 0; i < this->data_pins_.size(); i++)
    this->i80_bus_config_.data_gpio_nums[i] = this->data_pins_[i]->get_pin();

  ESP_LOGV(TAG, "Creating i80 bus...");
  if (esp_lcd_new_i80_bus(&i80_bus_config_, &this->i80_bus_handle_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed setting up the i80 bus...");
    this->mark_failed();
    return;
  }

  // Setup the panel
  this->i80_io_config_ = {.cs_gpio_num = this->cs_pin_->get_pin(),
                          .pclk_hz = this->pclk_hz_,
                          .trans_queue_depth = 10,
                          .on_color_trans_done = I8080Display::color_trans_done_handler,
                          .user_ctx = this,
                          .lcd_cmd_bits = this->cmd_bits_,
                          .lcd_param_bits = this->param_bits_,
                          .dc_levels =
                              {
                                  .dc_idle_level = 0,
                                  .dc_cmd_level = 0,
                                  .dc_dummy_level = 0,
                                  .dc_data_level = 1,
                              },
                          .flags = {
                              .reverse_color_bits = false,  // Reverse the data bits, D[N:0] -> D[0:N]
                              .swap_color_bytes = false,    // Swap can be done in LvGL (default) or DMA
                          }};

  ESP_LOGV(TAG, "Creating i80 IO handle...");
  if (esp_lcd_new_panel_io_i80(this->i80_bus_handle_, &this->i80_io_config_, &this->i80_io_handle_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed setting up the i80 panel io...");
    this->mark_failed();
    return;
  }

  this->i80_panel_config_ = {
      .reset_gpio_num = -1,
      //.color_space = ESP_LCD_COLOR_SPACE_RGB,
      //.bits_per_pixel = 16,
  };
  // Reset Pin
  if (this->reset_pin_ != nullptr) {
    this->i80_panel_config_.reset_gpio_num = this->reset_pin_->get_pin();
  }
  // Color Order
  switch (this->color_order_) {
    case display::ColorOrder::COLOR_ORDER_BGR:
      this->i80_panel_config_.color_space = ESP_LCD_COLOR_SPACE_BGR;
      break;
    default:
      this->i80_panel_config_.color_space = ESP_LCD_COLOR_SPACE_RGB;
      break;
  }
  // Color depth
  switch (this->buffer_color_mode_) {
    case ColorDepth::BITS_8:
      this->i80_panel_config_.bits_per_pixel = 8;
      break;
    default:
      this->i80_panel_config_.bits_per_pixel = 16;
      break;
  }
  // Call the chipset specific init (defined within the derived class)
  if (!this->init_chipset_()) {
    ESP_LOGE(TAG, "Failed setting up the chipset panel io...");
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "Resetting the panel...");
  esp_lcd_panel_reset(this->i80_panel_handle_);
  ESP_LOGV(TAG, "Initializing the panel...");
  esp_lcd_panel_init(this->i80_panel_handle_);
  // Set inversion, x/y coordinate order, x/y mirror according to your LCD module spec
  // the gap is LCD panel specific, even panels with the same driver IC, can have different gap value
  esp_lcd_panel_invert_color(this->i80_panel_handle_, this->invertcolors_);
  esp_lcd_panel_set_gap(this->i80_panel_handle_, this->offset_x_, this->offset_y_);
  esp_lcd_panel_mirror(this->i80_panel_handle_, this->mirror_x_, this->mirror_y_);
  esp_lcd_panel_swap_xy(this->i80_panel_handle_, this->swap_xy_);
  this->update();
  esp_lcd_panel_disp_on_off(this->i80_panel_handle_, true);
}

void I8080Display::on_shutdown(void) {
  esp_lcd_panel_del(this->i80_panel_handle_);
  esp_lcd_panel_io_del(this->i80_io_handle_);
  esp_lcd_del_i80_bus(this->i80_bus_handle_);
}

int I8080Display::get_height_internal() { return this->height_; }

int I8080Display::get_width_internal() { return this->width_; }

size_t I8080Display::get_buffer_length_() {
  if (this->buffer_color_mode_ == BITS_16) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * size_t(2);
  }
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal());
}

void I8080Display::alloc_buffer_() {
  ESP_LOGV(TAG, "Trying to allocate display buffer from:");
  ESP_LOGV(TAG, "Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
  ESP_LOGV(TAG, "Free SPIRAM: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
  ESP_LOGV(TAG, "Free heap DMA: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA));
  // Can't use DisplayBuffer::init_internal_(), because the buffer needs to be alligned for DMA.
  // This call will fail if PSRAM is not available.
  this->buffer_ = (uint8_t *) heap_caps_aligned_calloc(32, this->get_buffer_length_(), sizeof(uint8_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (this->buffer_ == nullptr) {
    this->mark_failed();
    ESP_LOGE(TAG, "Failed to allocate display buffer!");
    return;
  }
  ESP_LOGV(TAG, "Successfully allocated the display buffer.");
  ESP_LOGV(TAG, "Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
  ESP_LOGV(TAG, "Free SPIRAM: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
  ESP_LOGV(TAG, "Free heap DMA: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA));
  this->clear();
}

inline bool I8080Display::check_buffer_() {
  if (this->buffer_ == nullptr) {
    this->alloc_buffer_();
    return !this->is_failed();
  }
  return true;
}

void I8080Display::update() {
  // Prevent multiple DMA transfers from overlapping, or updating the display buffer
  // while a DMA transfer is in process.
  if (this->dma_in_process_)
    return;
  if (this->prossing_update_) {
    this->need_update_ = true;
    return;
  }
  this->prossing_update_ = true;
  do {
    this->need_update_ = false;
    this->do_update_();
  } while (this->need_update_);
  this->prossing_update_ = false;
  if (!this->needs_drawing_)
    return;
  esp_cache_msync(this->buffer_, this->get_buffer_length_(), 0x0);
  esp_lcd_panel_draw_bitmap(this->i80_panel_handle_, 0, 0, this->width_, this->height_, this->buffer_);
  this->needs_drawing_ = false;
}

void I8080Display::fill(Color color) {
  if (!this->check_buffer_())
    return;
  if (this->buffer_color_mode_ == ColorDepth::BITS_8) {
    const uint8_t color332 = display::ColorUtil::color_to_332(color);
    memset(this->buffer_, this->get_buffer_length_(), color332);
  } else {
    const uint16_t color565 = display::ColorUtil::color_to_565(color);
    for (uint32_t i = 0; i < this->get_buffer_length_(); i += 2) {
      this->buffer_[i] = (color565 >> 8) & 0xff;
      this->buffer_[i + 1] = color565 & 0xff;
    }
  }
  this->needs_drawing_ = true;
}

void I8080Display::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  switch (this->buffer_color_mode_) {
    case ColorDepth::BITS_8:
      this->buffer_[x + (y * this->width_)] = display::ColorUtil::color_to_332(color);
      break;

    default:
      const uint16_t color565 = display::ColorUtil::color_to_565(color);
      uint32_t pos = (x + (y * this->width_)) * 2;
      this->buffer_[pos] = (color565 >> 8) & 0xff;
      this->buffer_[pos + 1] = color565 & 0xff;
      break;
  }
  this->needs_drawing_ = true;
}

bool I8080Display::color_trans_done_handler(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                            void *user_ctx) {
  // Prevent multiple DMA transfers from overlapping.
  ((I8080Display *) user_ctx)->dma_in_process_ = false;
  return false;
}

//-----------   ST7789V display --------------
bool I8080ST7789V::init_chipset_() {
  ESP_LOGD(TAG, "Setting up ST7789V chipset using I8080 bus");
  if (esp_lcd_new_panel_st7789(this->i80_io_handle_, &this->i80_panel_config_, &this->i80_panel_handle_) != ESP_OK)
    return false;
  return true;
}

//-----------   NT35510 display --------------
bool I8080NT35510::init_chipset_() {
  ESP_LOGD(TAG, "Setting up NT35510 chipset using I8080 bus");
  if (esp_lcd_new_panel_nt35510(this->i80_io_handle_, &this->i80_panel_config_, &this->i80_panel_handle_) != ESP_OK)
    return false;
  return true;
}

}  // namespace i8080
}  // namespace esphome
