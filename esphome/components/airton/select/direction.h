#pragma once

#include "esphome/components/select/select.h"
#include "../airton.h"

namespace esphome {
namespace airton {

class VerticalDirectionSelect : public select::Select, public Parented<AirtonClimate> {
 public:
  VerticalDirectionSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace airton
}  // namespace esphome
