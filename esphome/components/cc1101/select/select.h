#pragma once

#include "esphome/components/select/select.h"
#include "../cc1101.h"

namespace esphome {
namespace cc1101 {

class RxAttenuationSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_rx_attenuation((RxAttenuation) *index);
    }
  }
};

class TunerModulationSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_tuner_modulation((Modulation) *index);
    }
  }
};

class AgcMagnTargetSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_magn_target((MagnTarget) *index);
    }
  }
};

class AgcMaxLnaGainSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_max_lna_gain((MaxLnaGain) *index);
    }
  }
};

class AgcMaxDvaGainSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_max_dva_gain((MaxDvgaGain) *index);
    }
  }
};

class AgcCarrierSenseRelThrSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_carrier_sense_rel_thr((CarrierSenseRelThr) *index);
    }
  }
};

class AgcFilterLengthFskMskSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_filter_length_fsk_msk((FilterLengthFskMsk) *index);
    }
  }
};

class AgcFilterLengthAskOokSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_filter_length_ask_ook((FilterLengthAskOok) *index);
    }
  }
};

class AgcFreezeSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_freeze((Freeze) *index);
    }
  }
};

class AgcWaitTimeSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_wait_time((WaitTime) *index);
    }
  }
};

class AgcHystLevelSelect : public select::Select, public Parented<CC1101Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_agc_hyst_level((HystLevel) *index);
    }
  }
};

}  // namespace cc1101
}  // namespace esphome
