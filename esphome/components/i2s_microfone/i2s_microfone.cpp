#include "i2s_microfone.h"
#include "esphome/core/log.h"

extern "C" {
#include "driver/i2s.h"
}

namespace esphome {
namespace i2s_microfone {

static const char *const TAG = "i2s_microfone";

void I2SMicrofoneSensor::setup() {
  ESP_LOGI(TAG, "Configuring the I2S driver with pins: bck=%d, ws=%d, data_in=%d",
           this->bck_pin_->get_pin(), this->ws_pin_->get_pin(), this->data_in_pin_->get_pin());

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {
      .bck_io_num = this->bck_pin_->get_pin(),
      .ws_io_num = this->ws_pin_->get_pin(),
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = this->data_in_pin_->get_pin()};

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void I2SMicrofoneSensor::update() {
  uint8_t buffer[64];
  size_t bytes_read;

  i2s_read(I2S_NUM_0, &buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

  int32_t *samples = reinterpret_cast<int32_t *>(buffer);
  size_t sample_count = bytes_read / sizeof(int32_t);

  float avg = 0;
  for (size_t i = 0; i < sample_count; i++) {
    avg += abs(samples[i]);
  }

  avg /= sample_count;

  float db = 20.0f * log10f(avg);
  float offset = this->calibration_offset_;
  if (this->calibration_number_ != nullptr) {
    offset = this->calibration_number_->state;
  }

  db += offset;
  db = esphome::clamp(db, 0.0f, 120.0f);


  ESP_LOGD(TAG, "Noise level: %.1f dB", db);
  this->publish_state(db);
}

}
}
