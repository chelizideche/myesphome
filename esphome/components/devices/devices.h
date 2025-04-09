#pragma once

namespace esphome {
namespace devices {

class SubDevice {
 public:
  void set_name(std::string name) { name_ = std::move(name); }
  std::string get_name() { return name_; }
  void set_area(std::string area) { area_ = std::move(area); }
  std::string get_area() { return area_; }

 protected:
  // std::string id_ = "";
  std::string name_ = "";
  std::string area_ = "";
};

}  // namespace devices
}  // namespace esphome
