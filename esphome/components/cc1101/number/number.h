#pragma once

#include "esphome/components/number/number.h"
#include "../cc1101.h"
#include <cmath>

namespace esphome {
namespace cc1101 {

class TunerFrequencyNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_frequency(value);
  }
};

class TunerIfFrequencyNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_if_frequency(value);
  }
};

class TunerBandwidthNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_bandwidth(value);
  }
};

class TunerChannelNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_channel((uint8_t) lround(value));
  }
};

class TunerChannelSpacingNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_channel_spacing(value);
  }
};

class TunerFskDeviationNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_fsk_deviation(value);
  }
};

class TunerMskDeviationNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_msk_deviation((uint8_t) lround(value));
  }
};

class TunerSymbolRateNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_symbol_rate(value);
  }
};

class AgcCarrierSenseAbsThrNumber : public number::Number, public Parented<CC1101Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_agc_carrier_sense_abs_thr((int8_t) lround(value));
  }
};

}  // namespace cc1101
}  // namespace esphome
