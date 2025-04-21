#pragma once

#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

#include <hal/adc_types.h>
#include <esp_adc/adc_continuous.h>

namespace esphome {
namespace adc_microphone {

class ADCAudioMicrophone : public microphone::Microphone, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;

  void loop() override;

  /** Reads audio data into a buffer
   *
   * @param buf A preallocated buffer to read data into
   * @param len the length (in BYTES) of the buffer
   * @return The number of BYTES actually read
   */
  size_t read(int16_t *buf, size_t len) override;

  void set_adc_channel(int gpio_pin);
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_attenuation(adc_atten_t attenuation) { this->attenuation_ = attenuation; }

 protected:
  void start_();
  void stop_();
  void read_();

  adc_unit_t adc_unit_;
  adc_channel_t adc_channel_;

  HighFrequencyLoopRequester high_freq_;
  uint32_t sample_rate_;
  adc_atten_t attenuation_;
  adc_continuous_handle_t adc_handle_;
  uint8_t *dma_out_buffer_;

  // keep the output vector hanging around rather than constantly reallocating
  std::vector<int16_t> samples_;
};

}  // namespace adc_microphone
}  // namespace esphome

#endif