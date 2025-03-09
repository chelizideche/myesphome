
#pragma once

#include <iomanip>
#include <map>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_AUDIO_DAC
#include "esphome/components/es8388/audio_dac/es8388.h"

#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome {
namespace es8388 {

enum DacOutputLineStructure : uint8_t { DAC_OUTPUT_LINE1 = 0, DAC_OUTPUT_LINE2 = 1, DAC_OUTPUT_BOTH = 2 };

// Convert zone type int to enum
static const std::map<DacOutputLineStructure, std::string> DAC_OUTPUT_INT_TO_ENUM{
    {DAC_OUTPUT_LINE1, "LINE1"}, {DAC_OUTPUT_LINE2, "LINE2"}, {DAC_OUTPUT_BOTH, "BOTH"}};

// Convert zone type enum to int
static const std::map<std::string, uint8_t> DAC_OUTPUT_ENUM_TO_INT{
    {"LINE1", DAC_OUTPUT_LINE1}, {"LINE2", DAC_OUTPUT_LINE2}, {"BOTH", DAC_OUTPUT_BOTH}};

class ES8388Hub : public Component {
#ifdef USE_SELECT
  SUB_SELECT(dac_output)
#endif

 public:
  ES8388Hub();
  void setup() override;
  void dump_config() override;

#ifdef USE_SELECT
  void set_dac_output(const std::string &state);
#endif

#ifdef USE_AUDIO_DAC
  void register_audio_dac(es8388::ES8388 *obj) { this->audio_dac_ = obj; }
#endif

 protected:
#ifdef USE_SELECT
  es8388::ES8388 *audio_dac_;
  uint8_t dac_output_ = 0;
#endif
};

}  // namespace es8388
}  // namespace esphome
