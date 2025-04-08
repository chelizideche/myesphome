#pragma once

namespace esphome {
namespace devices {

class SubDevice {
 public:
  void set_name(std::string name) { name_ = name; }
  std::string get_name(void) { return name_; }

 protected:
  // std::string id_ = "";
  std::string name_ = "";
  std::string suggested_area_ = "";
};

}  // namespace devices
}  // namespace esphome
