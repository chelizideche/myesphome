#pragma once

#include <cstdint>

namespace esphome {

class Area {
 public:
  void set_area_id(uint32_t area_id) { area_id_ = area_id; }
  uint32_t get_area_id() { return area_id_; }
  void set_name(const char *name) { name_ = name; }
  const char *get_name() { return name_; }

 protected:
  uint32_t area_id_{};
  const char *name_ = "";
};

}  // namespace esphome
