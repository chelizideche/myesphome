#include "LD2410SNumber.h"

namespace esphome {
namespace ld2410s {
void LD2410SDelayNumber::control(float delay) {
  this->publish_state(delay);
  this->parent_->set_delay(delay);
}

void LD2410SDistReportingFreqNumber::control(float distance_reporting_freq) {
  this->publish_state(distance_reporting_freq);
  this->parent_->set_distance_reporting_freq(distance_reporting_freq);
}

void LD2410SMaxDistanceNumber::control(float max_distance) {
  this->publish_state(max_distance);
  this->parent_->set_max_distance(max_distance);
}

void LD2410SMinDistanceNumber::control(float min_distance) {
  this->publish_state(min_distance);
  this->parent_->set_min_distance(min_distance);
}

void LD2410SStatusReportingFreqNumber::control(float status_reporting_freq) {
  this->publish_state(status_reporting_freq);
  this->parent_->set_status_reporting_freq(status_reporting_freq);
}

void LD2410STriggerSelectedGateNumber::control(float trigger_selected_gate) {
  this->publish_state(trigger_selected_gate);
  this->parent_->set_trigger_selected_gate(trigger_selected_gate);
}

void LD2410STriggerThresholdNumber::control(float trigger_threshold) {
  this->publish_state(trigger_threshold);
  this->parent_->set_trigger_threshold(trigger_threshold);
}

void LD2410STriggerHoldNumber::control(float trigger_hold) {
  this->publish_state(trigger_hold);
  this->parent_->set_trigger_hold(trigger_hold);
}

void LD2410STriggerSnrNumber::control(float trigger_snr) {
  this->publish_state(trigger_snr);
  this->parent_->set_trigger_snr(trigger_snr);
}

}  // namespace ld2410s
}  // namespace esphome
