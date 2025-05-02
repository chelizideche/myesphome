#include "adc_audio_microphone.h"

#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc_microphone {

// The number of SAMPLES to buffer
static const size_t BUFFER_SIZE = 1024;

static const char *const TAG = "adc_microphone";

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define DMA_FORMAT_TYPE_1 1
#define DMA_FORMAT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#else
#define DMA_FORMAT_TYPE_2 1
#define DMA_FORMAT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#endif

#define ADC_ESP_ERROR_CHECK(err, action, retval) \
  if (unlikely((err) != ESP_OK)) { \
    ESP_LOGE(TAG, "Could not " action " for ADC Microphone: %s", esp_err_to_name(err)); \
    this->status_set_error("Failed to " action); \
    return retval; \
  }

// 16 samples seems like a decent enough size for each frame
// increased size to prevent overflows (maybe?)
#define DMA_SAMPLES_PER_FRAME 64

// this is defined by codegen, also define here for testing
#ifndef ADC_DMA_NUM_FRAMES
#define ADC_DMA_NUM_FRAMES 16
#endif

static const uint16_t DMA_FRAME_SIZE = (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * DMA_SAMPLES_PER_FRAME);
static const uint16_t DMA_BUF_SIZE = (DMA_FRAME_SIZE * ADC_DMA_NUM_FRAMES);

static bool IRAM_ATTR s_conv_overflow_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                         void *user_data) {
  ESP_DRAM_LOGE(DRAM_STR("adc_microhpone"), "ADC Conversion Buffer Overflow");
  return true;
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                     void *user_data) {
  ESP_DRAM_LOGV(DRAM_STR("adc_microhpone"), "ADC mic has data");
  return true;
}

void ADCAudioMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC Audio Microphone...");
  // it's arguable that some of this should be moved into start(), not setup()

  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = DMA_BUF_SIZE, .conv_frame_size = DMA_FRAME_SIZE,
      // .flags = 0, // leave flags default-initialized to zero
  };

  ADC_ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle_), "init handle", );
}

void ADCAudioMicrophone::set_adc_channel(int gpio_pin) {
  ADC_ESP_ERROR_CHECK(adc_continuous_io_to_channel(gpio_pin, &adc_unit_, &adc_channel_), "get ADC channel", );
}

void ADCAudioMicrophone::start() {
  ESP_LOGI(TAG, "ADC Mic start Requested");
  if (this->is_failed())
    return;
  if (this->state_ == microphone::STATE_RUNNING)
    return;  // Already running
  this->state_ = microphone::STATE_STARTING;
}

void ADCAudioMicrophone::start_() {
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
      .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
  };

  ESP_LOGI(TAG, "Mic setup with %" PRIu8 " atten, %" PRIu8 "channel, %" PRIu8 " unit, and bitwidth %" PRIu8,
           adc_pattern.atten, adc_pattern.channel, adc_pattern.unit, adc_pattern.bit_width);

  dig_cfg.adc_pattern = &adc_pattern;

  ADC_ESP_ERROR_CHECK(adc_continuous_config(adc_handle_, &dig_cfg), "configure continuous mode", );

  auto conv_done_callback = s_conv_done_cb;
  if (esp_log_level_get(TAG) < ESP_LOG_VERBOSE) {
    // If the message won't be logged, don't do the callback
    conv_done_callback = nullptr;
  }

  adc_continuous_evt_cbs_t cbs = {
      .on_conv_done = s_conv_done_cb,
      .on_pool_ovf = s_conv_overflow_cb,
  };

  ADC_ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle_, &cbs, NULL), "setup error callbacks", );

  ADC_ESP_ERROR_CHECK(adc_continuous_start(adc_handle_), "start microphone data collection", );

  dma_out_buffer_ = new uint8_t[BUFFER_SIZE * sizeof(adc_digi_output_data_t)];

  this->audio_stream_info_ = audio::AudioStreamInfo(16, 1, this->sample_rate_);

  this->samples_.reserve(BUFFER_SIZE);
  this->state_ = microphone::STATE_RUNNING;
  this->high_freq_.start();
  this->status_clear_error();
  ESP_LOGI(TAG, "ADC Mic Started");
}

void ADCAudioMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;
  if (this->state_ == microphone::STATE_STARTING) {
    this->state_ = microphone::STATE_STOPPED;
    return;
  }
  this->state_ = microphone::STATE_STOPPING;
  ESP_LOGI(TAG, "ADC Mic Stop Requested");
}

void ADCAudioMicrophone::stop_() {
  ESP_LOGI(TAG, "ADC Mic Stopped");
  ADC_ESP_ERROR_CHECK(adc_continuous_stop(adc_handle_), "stop ADC microphone", );

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
  // just flush the pool rather than fully deinitializing the driver
  ADC_ESP_ERROR_CHECK(adc_continuous_flush_pool(adc_handle_), "flush ADC microphone pool", );
#else
  // Can't just call deinit, since that destroys the whole handle.
  //  ADC_ESP_ERROR_CHECK(adc_continuous_deinit(adc_handle_), "deinit ADC microphone pool", );
  ESP_LOGI(TAG, "ESP_IDF version too low to flush ADC microphone buffer on stop");
#endif

  delete[] dma_out_buffer_;
  this->samples_.clear();
  this->samples_.shrink_to_fit();
  this->state_ = microphone::STATE_STOPPED;
  this->high_freq_.stop();
  this->status_clear_error();
}

void ADCAudioMicrophone::read_() {
  // NOTE: Could acheive a possible speedup with
  // https://hackingcpp.com/cpp/recipe/uninitialized_numeric_array.html#no_init
  // (ie, avoiding the possible 0-initializing of the end of the buffer).
  samples_.resize(BUFFER_SIZE * sizeof(uint16_t));
  uint32_t bytes_read = 0;
  auto out_buffer = samples_.data();

  // We have an intermediate buffer; we can't read more data than is available in it
  size_t max_read = BUFFER_SIZE * sizeof(adc_digi_output_data_t);
  ADC_ESP_ERROR_CHECK(adc_continuous_read(adc_handle_, dma_out_buffer_, max_read, &bytes_read, 0),
                      "read data from buffer", );
  ESP_LOGD(TAG, "read %" PRIu32 " of maximum %zu bytes from ADC", bytes_read, max_read);

  if (bytes_read == 0) {
    this->status_set_warning("Zero bytes read from ADC");
    return;
  }
  this->status_clear_warning();

  // remove the extra data (channel, etc), only leaving what we care about
  // skip calibration, since that looks performance-intensive
  size_t samples_read = bytes_read / SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
  adc_digi_output_data_t *input_buf = reinterpret_cast<adc_digi_output_data_t *>(dma_out_buffer_);
  int16_t seen_nonzero = 0;
  ESP_LOGVV(TAG, "First sample is %" PRIX32, input_buf[0].val);
  auto buf_writing = reinterpret_cast<uint16_t *>(out_buffer);
  for (size_t i = 0; i < samples_read; i++) {
#ifdef DMA_FORMAT_TYPE_1
    buf_writing[i] = input_buf[i].type1.data;
#elif DMA_FORMAT_TYPE_2
    buf_writing[i] = input_buf[i].type2.data;
#else
#error "Unknown DMA_FORMAT_TYPE"
#endif
    seen_nonzero |= (buf_writing[i]);
  }
  if (seen_nonzero == 0) {
    this->status_set_warning("All-zero data from ADC; confirm your connections!");
  }

  samples_.resize(samples_read * sizeof(uint16_t));
  ESP_LOGV(TAG, "Processing %zu ADC samples", samples_read);
  this->data_callbacks_.call(samples_);
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
