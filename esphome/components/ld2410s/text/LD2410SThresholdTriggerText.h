#pragma once

#include "../LD2410S.h"
#include "esphome/core/component.h"
#include "esphome/components/text/text.h"

namespace esphome {
namespace ld2410s {

// class LD2410SText : public text::Text, public Component {

class LD2410SThresholdTriggerText : public text::Text, public Parented<LD2410S> {
 public:
  LD2410SThresholdTriggerText() = default;
  // void set_source(text::Text *source) { source_ = source; }
  // void setup() override;
  // void dump_config() override;
  // float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

  // text::Text *source_;
};

}  // namespace ld2410s
}  // namespace esphome
