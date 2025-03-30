#pragma once

#include "../LD2410S.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {
class LD2410SDelayNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SDelayNumber() = default;

 protected:
  void control(float no_delay) override;
};
class LD2410SDistReportingFreqNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SDistReportingFreqNumber() = default;

 protected:
  void control(float distance_reporting_freq) override;
};
class LD2410SMaxDistanceNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SMaxDistanceNumber() = default;

 protected:
  void control(float max_distance) override;
};
class LD2410SMinDistanceNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SMinDistanceNumber() = default;

 protected:
  void control(float min_distance) override;
};
class LD2410SStatusReportingFreqNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SStatusReportingFreqNumber() = default;

 protected:
  void control(float status_reporting_freq) override;
};
class LD2410STriggerHoldNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410STriggerHoldNumber() = default;

 protected:
  void control(float trigger_hold) override;
};
class LD2410STriggerSelectedGateNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410STriggerSelectedGateNumber() = default;

 protected:
  void control(float trigger_selected_gate) override;
};
class LD2410STriggerSnrNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410STriggerSnrNumber() = default;

 protected:
  void control(float trigger_snr) override;
};
class LD2410STriggerThresholdNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410STriggerThresholdNumber() = default;

 protected:
  void control(float trigger_threshold) override;
};

}  // namespace ld2410s
}  // namespace esphome