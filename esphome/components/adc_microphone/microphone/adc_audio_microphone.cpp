#include "adc_audio_microphone.h"

#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc_microphone {

static const size_t BUFFER_SIZE = 512;

static const char *const TAG = "adc_microphone";

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define DMA_FORMAT_TYPE_1 1
#define DMA_FORMAT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#else
#define DMA_FORMAT_TYPE_2 1
#define DMA_FORMAT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#endif

// 16 samples seems like a decent enough size for each frame
#define DMA_SAMPLES_PER_FRAME 16

// ensure buffer is large enough to work even when running at slower speed
#define DMA_FRAME_SIZE SOC_ADC_DIGI_DATA_BYTES_PER_CONV *DMA_SAMPLES_PER_FRAME
#define DMA_BUF_SIZE DMA_FRAME_SIZE * 100

void ADCAudioMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC Audio Microphone...");
  // it's arguable that some of this should be moved into start(), not setup()

  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = DMA_BUF_SIZE,
      .conv_frame_size = DMA_FRAME_SIZE,
  };

  ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

  adc_digi_output_format_t format = DMA_FORMAT_TYPE;

  adc_digi_convert_mode_t adc_conv_mode = (adc_unit_ == ADC_UNIT_1) ? ADC_CONV_SINGLE_UNIT_1 : ADC_CONV_SINGLE_UNIT_2;

  adc_continuous_config_t dig_cfg = {
      .pattern_num = 1,
      .sample_freq_hz = sample_rate_,
      .conv_mode = adc_conv_mode,
      .format = format,
  };

  adc_digi_pattern_config_t adc_pattern = {
      .atten = attenuation_,
      .channel = adc_channel_,
      .unit = adc_unit_,
      // For some reason, the continuous mode driver doesn't support ADC_BITWIDTH_DEFAULT.
      // Use 12 instead and hope for the best.
      .bit_width = 12,
  };
  dig_cfg.adc_pattern = &adc_pattern;
  ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
}

void ADCAudioMicrophone::set_adc_channel(int gpio_pin) {
  ESP_ERROR_CHECK(adc_continuous_io_to_channel(gpio_pin, &adc_unit_, &adc_channel_));
}

void ADCAudioMicrophone::start() {
  if (this->is_failed())
    return;
  if (this->state_ == microphone::STATE_RUNNING)
    return;  // Already running
  this->state_ = microphone::STATE_STARTING;
}
void ADCAudioMicrophone::start_() {
  esp_err_t err;
  {
    err = adc_continuous_start(adc_handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error starting ADC microphone: %s", esp_err_to_name(err));
      this->status_set_error("Cound not start");
      return;
    }
    // two calls are needed to empty the buffer anyways when it fills,
    // so lets save some memory
    dma_out_buffer = new uint8_t[DMA_BUF_SIZE / 2];
  }
  this->state_ = microphone::STATE_RUNNING;
  this->high_freq_.start();
  this->status_clear_error();
}

void ADCAudioMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;
  if (this->state_ == microphone::STATE_STARTING) {
    this->state_ = microphone::STATE_STOPPED;
    return;
  }
  this->state_ = microphone::STATE_STOPPING;
}

void ADCAudioMicrophone::stop_() {
  esp_err_t err;

  err = adc_continuous_stop(adc_handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error stopping ADC microphone: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }
  delete[] dma_out_buffer;
  this->state_ = microphone::STATE_STOPPED;
  this->high_freq_.stop();
  this->status_clear_error();
}

size_t ADCAudioMicrophone::read(int16_t *buf, size_t len) {
  uint32_t bytes_read = 0;
  size_t max_read = std::min(len, (size_t) DMA_BUF_SIZE / 2);
  esp_err_t err = adc_continuous_read(adc_handle, dma_out_buffer, max_read, &bytes_read, 4);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error reading from adc microphone: %s", esp_err_to_name(err));
    this->status_set_warning();
    return 0;
  }
  if (bytes_read == 0) {
    this->status_set_warning();
    return 0;
  }
  this->status_clear_warning();

  // remove the extraneous data, only leaving what we care about
  // skip calibration, since that looks performance-intensive
  size_t samples_read = bytes_read / SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
  adc_digi_output_data_t *temp = reinterpret_cast<adc_digi_output_data_t *>(dma_out_buffer);

  for (size_t i = 0; i < samples_read; i++) {
#ifdef DMA_FORMAT_TYPE_1
    buf[i] = temp[i].type1.data;
#elif DMA_FORMAT_TYPE_2
    buf[i] = temp[i].type2.data;
#else
#error "Unknown DMA_FORMAT_TYPE"
#endif
  }
  return samples_read * sizeof(int16_t);
}

void ADCAudioMicrophone::read_() {
  std::vector<int16_t> samples;
  samples.resize(BUFFER_SIZE);
  size_t bytes_read = this->read(samples.data(), BUFFER_SIZE / sizeof(int16_t));
  samples.resize(bytes_read / sizeof(int16_t));
  this->data_callbacks_.call(samples);
}

void ADCAudioMicrophone::loop() {
  switch (this->state_) {
    case microphone::STATE_STOPPED:
      break;
    case microphone::STATE_STARTING:
      this->start_();
      break;
    case microphone::STATE_RUNNING:
      if (this->data_callbacks_.size() > 0) {
        this->read_();
      }
      break;
    case microphone::STATE_STOPPING:
      this->stop_();
      break;
  }
}

}  // namespace adc_microphone
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ESP_IDF
