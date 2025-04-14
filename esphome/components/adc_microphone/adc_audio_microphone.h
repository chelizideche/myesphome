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

  size_t read(int16_t *buf, size_t len) override;

  void set_adc_channel(int gpio_pin) { adc_continuous_io_to_channel(gpio_pin, &adc_unit_, &adc_channel_); }
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
};

}  // namespace adc_microphone
}  // namespace esphome

#endif