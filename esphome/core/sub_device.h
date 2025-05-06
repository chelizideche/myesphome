#pragma once

#include "esphome/core/string_ref.h"

namespace esphome {

class SubDevice {
 public:
  void set_uid(uint32_t uid) { uid_ = uid; }
  uint32_t get_uid() { return uid_; }
  void set_name(std::string name) { name_ = std::move(name); }
  std::string get_name() { return name_; }
  void set_area(std::string area) { area_ = std::move(area); }
  std::string get_area() { return area_; }

 protected:
  uint32_t uid_{};
  std::string name_ = "";
  std::string area_ = "";
};

}  // namespace esphome
