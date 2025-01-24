#include "adc_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc {

static const char *const TAG = "adc.common";

enum class SamplingMode : uint8_t { AVG = 0, MIN = 1, MAX = 2 };

Aggregator::Aggregator(SamplingMode mode) {
  mode_ = mode;
  // set to max uint if mode is "min"
  if (mode == SamplingMode::MIN) {
    aggr_ = UINT32_MAX;
  }
}

void Aggregator::add_sample(uint32_t value) {
  if (mode_ == SamplingMode::AVG) {
    // avg
    aggr_ += value;
  } else if (mode_ == SamplingMode::MIN) {
    // min
    if (value < aggr_) {
      aggr_ = value;
    }
  } else {
    // max
    if (value > aggr_) {
      aggr_ = value;
    }
  }

  samples_ += 1;
}

uint32_t Aggregator::aggregate() {
  if (this->mode_ == SamplingMode::AVG) {
    return (aggr_ + (this->samples_ >> 1)) / this->samples_;  // NOLINT(clang-analyzer-core.DivideZero)
  }

  return aggr_;
}

void ADCSensor::update() {
  float value_v = this->sample();
  ESP_LOGV(TAG, "'%s': Got voltage=%.4fV", this->get_name().c_str(), value_v);
  this->publish_state(value_v);
}

void ADCSensor::set_sample_count(uint8_t sample_count) {
  if (sample_count != 0) {
    this->sample_count_ = sample_count;
  }
}

void ADCSensor::set_sampling_mode(SamplingMode sampling_mode) { this->sampling_mode_ = sampling_mode; }

float ADCSensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace adc
}  // namespace esphome
