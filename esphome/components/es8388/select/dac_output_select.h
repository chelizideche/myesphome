#pragma once

#include "esphome/components/select/select.h"
#include "esphome/components/es8388/es8388_hub.h"

namespace esphome {
namespace es8388 {

class DacOutputSelect : public select::Select, public Parented<ES8388Hub> {
 public:
  DacOutputSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace es8388
}  // namespace esphome
