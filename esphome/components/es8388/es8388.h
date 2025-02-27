#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es8388 {

class ES8388 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  /////////////////////////
  // Component overrides //
  /////////////////////////

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

  ////////////////////////
  // AudioDac overrides //
  ////////////////////////

  /// @brief Writes the volume out to the DAC
  /// @param volume floating point between 0.0 and 1.0
  /// @return True if successful and false otherwise
  bool set_volume(float volume) override;

  /// @brief Gets the current volume out from the DAC
  /// @return floating point between 0.0 and 1.0
  float volume() override;

  /// @brief Disables mute for audio out
  /// @return True if successful and false otherwise
  bool set_mute_off() override { return this->set_mute_state_(false); }

  /// @brief Enables mute for audio out
  /// @return True if successful and false otherwise
  bool set_mute_on() override { return this->set_mute_state_(true); }

  bool is_muted() override { return this->is_muted_; }

 protected:
  /// @brief Mutes or unmutes the DAC audio out
  /// @param mute_state True to mute, false to unmute
  /// @return True if successful and false otherwise
  bool set_mute_state_(bool mute_state);
};

using es_adc_input_t = enum {
  ADC_INPUT_MIN = -1,
  ADC_INPUT_LINPUT1_RINPUT1 = 0x00,
  ADC_INPUT_MIC1 = 0x05,
  ADC_INPUT_MIC2 = 0x06,
  ADC_INPUT_LINPUT2_RINPUT2 = 0x50,
  ADC_INPUT_DIFFERENCE = 0xf0,
  ADC_INPUT_MAX,
};

using es_dac_output_t = enum {
  DAC_OUTPUT_MIN = -1,
  DAC_OUTPUT_LOUT1 = 0x04,
  DAC_OUTPUT_LOUT2 = 0x08,
  DAC_OUTPUT_SPK = 0x09,
  DAC_OUTPUT_ROUT1 = 0x10,
  DAC_OUTPUT_ROUT2 = 0x20,
  DAC_OUTPUT_ALL = 0x3c,
  DAC_OUTPUT_MAX,
};

}  // namespace es8388
}  // namespace esphome
