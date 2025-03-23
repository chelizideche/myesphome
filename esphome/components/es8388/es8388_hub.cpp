#include "es8388_hub.h"
#include "esphome/core/log.h"

namespace esphome {
namespace es8388 {

static const char *const TAG = "ES8388Hub";

ES8388Hub::ES8388Hub() {}

void ES8388Hub::setup() {
#ifdef USE_SELECT

  static uint8_t dac_power = this->audio_dac_->get_dac_power();
  static uint8_t dac_output = 0;

  switch (dac_power) {
    case ES8388_DAC_OUTPUT_LOUT1_ROUT1:
      dac_output = DAC_OUTPUT_LINE1;
      break;
    case ES8388_DAC_OUTPUT_LOUT2_ROUT2:
      dac_output = DAC_OUTPUT_LINE2;
      break;
    case ES8388_DAC_OUTPUT_BOTH:
      dac_output = DAC_OUTPUT_BOTH;
      break;
    default:
      break;
  };

  std::string dac_output_string = DAC_OUTPUT_INT_TO_ENUM.at(static_cast<DacOutputLineStructure>(dac_output));

  if (this->dac_output_select_ != nullptr) {
    this->dac_output_select_->publish_state(dac_output_string);
  }

#endif
}

void ES8388Hub::set_dac_output(const std::string &state) {
  this->dac_output_ = DAC_OUTPUT_ENUM_TO_INT.at(state);
  static uint8_t dac_power = 0;

  switch (this->dac_output_) {
    case DAC_OUTPUT_LINE1:
      dac_power = ES8388_DAC_OUTPUT_LOUT1_ROUT1;
      break;
    case DAC_OUTPUT_LINE2:
      dac_power = ES8388_DAC_OUTPUT_LOUT2_ROUT2;
      break;
    case DAC_OUTPUT_BOTH:
      dac_power = ES8388_DAC_OUTPUT_BOTH;
      break;
    default:
      break;
  };

  // Set es8388 dac power
  this->audio_dac_->set_dac_power(dac_power);
}

void ES8388Hub::dump_config() {
#ifdef USE_SELECT
  LOG_SELECT("  ", "DacOutputSelect", this->dac_output_select_);

#endif
}

}  // namespace es8388
}  // namespace esphome
