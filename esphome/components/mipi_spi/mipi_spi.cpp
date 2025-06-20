#include "mipi_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mipi_spi {

void MipiSpi::setup() {
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
  this->setup_complete_ = true;
  this->init_sequence_.clear();
  ESP_LOGCONFIG(TAG, "MIPI SPI setup complete");
}

void MipiSpi::update() {
  if (!this->setup_complete_ || this->is_failed()) {
    return;
  }
  this->do_update_();
  if (this->buffer_ == nullptr || this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
    return;
  ESP_LOGV(TAG, "x_low %d, y_low %d, x_high %d, y_high %d", this->x_low_, this->y_low_, this->x_high_, this->y_high_);
  // Some chips require that the drawing window be aligned on certain boundaries
  auto dr = this->draw_rounding_;
  this->x_low_ = this->x_low_ / dr * dr;
  this->y_low_ = this->y_low_ / dr * dr;
  this->x_high_ = (this->x_high_ + dr) / dr * dr - 1;
  this->y_high_ = (this->y_high_ + dr) / dr * dr - 1;
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;
  this->write_to_display_(this->x_low_, this->y_low_, w, h, this->buffer_, this->x_low_, this->y_low_,
                          this->width_ - w - this->x_low_);
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

void MipiSpi::reset_params_() {
  if (!this->is_ready())
    return;
  this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
  if (this->brightness_.has_value())
    this->write_command_(BRIGHTNESS, this->brightness_.value());
}

void MipiSpi::write_init_sequence_() {
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
  this->setup_complete_ = true;
  ESP_LOGCONFIG(TAG, "MIPI SPI setup complete");
}

void MipiSpi::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  ESP_LOGVV(TAG, "Set addr %d/%d, %d/%d", x1, y1, x2, y2);
  uint8_t buf[4];
  x1 += this->offset_width_;
  x2 += this->offset_width_;
  y1 += this->offset_height_;
  y2 += this->offset_height_;
  put16_be(buf, y1);
  put16_be(buf + 2, y2);
  this->write_command_(RASET, buf, sizeof buf);
  put16_be(buf, x1);
  put16_be(buf + 2, x2);
  this->write_command_(CASET, buf, sizeof buf);
}

void MipiSpi::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
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

void MipiSpi::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MIPI_SPI Display\n"
                "  Model: %s\n"
                "  Width: %u\n"
                "  Height: %u",
                this->model_, this->width_, this->height_);
  if (this->offset_width_ != 0)
    ESP_LOGCONFIG(TAG, "  Offset width: %u", this->offset_width_);
  if (this->offset_height_ != 0)
    ESP_LOGCONFIG(TAG, "  Offset height: %u", this->offset_height_);
  ESP_LOGCONFIG(TAG,
                "  Swap X/Y: %s\n"
                "  Mirror X: %s\n"
                "  Mirror Y: %s\n"
                "  Invert colors: %s\n"
                "  Color order: %s\n"
                "  Buffer pixels: %d bits\n"
                "  Display pixels: %d bits",
                YESNO(this->madctl_ & MADCTL_MV), YESNO(this->madctl_ & (MADCTL_MX | MADCTL_XFLIP)),
                YESNO(this->madctl_ & (MADCTL_MY | MADCTL_YFLIP)), YESNO(this->invert_colors_),
                this->madctl_ & MADCTL_BGR ? "BGR" : "RGB", this->get_buffer_bits_(), this->get_display_bits_());
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

}  // namespace mipi_spi
}  // namespace esphome
