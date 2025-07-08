#pragma once

#include <string>
#include <unordered_map>

namespace esphome {
namespace camera {

/** CameraIncrementalContext is a presistent state object used across
 *  multiple stages camera processing (capture and overlay).
 *  The 'done' flag controls incremental flow. It is set to 'true' by default
 *  before user code is called. If the user wants to continue processing
 *  in the next cycle, they must explicitly set 'done = false' inside the automation.
 */
struct CameraIncrementalContext {
  enum ValueType : uint8_t {
    VALUE_TYPE_NONE = 0,
    VALUE_TYPE_INT,
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_BOOL,
    VALUE_TYPE_STRING
  };
  struct Value {
    ValueType value_type_ = VALUE_TYPE_NONE;
    union {
      int i_;
      float f_;
      bool b_;
    };
    std::string s_;
    Value() : value_type_(ValueType::VALUE_TYPE_NONE), i_(0) {}
    Value(int value) : value_type_(ValueType::VALUE_TYPE_INT), i_(value) {}
    Value(float value) : value_type_(ValueType::VALUE_TYPE_FLOAT), f_(value) {}
    Value(bool value) : value_type_(ValueType::VALUE_TYPE_BOOL), b_(value) {}
    Value(std::string value) : value_type_(ValueType::VALUE_TYPE_STRING), s_(std::move(std::move(value))) {}
    Value(const char *value) : value_type_(ValueType::VALUE_TYPE_STRING), s_(value) {}
  };
  // Common coordinates
  int x = 0;
  int y = 0;
  int z = 0;
  // State of arbitrary state-machine
  int state = 0;
  // Indicates if incremental processing is finished.
  // Reset to false to leave capture or overlay states.
  bool done = true;
  // Sets user specific persisten data
  void set(const std::string &key, const int value) {
    Value v(value);
    values_[key] = std::move(v);
  }
  void set(const std::string &key, const float value) {
    Value v(value);
    values_[key] = std::move(v);
  }
  void set(const std::string &key, const bool value) {
    Value v(value);
    values_[key] = std::move(v);
  }
  void set(const std::string &key, const std::string &value) {
    Value v(value);
    values_[key] = std::move(v);
  }
  void set(const std::string &key, const char *value) {
    Value v(value);
    values_[key] = std::move(v);
  }
  bool get(const std::string &key, int *out) const {
    auto it = values_.find(key);
    if (it == values_.end() || it->second.value_type_ != ValueType::VALUE_TYPE_INT)
      return false;

    *out = it->second.i_;
    return true;
  }
  bool get(const std::string &key, float *out) const {
    auto it = values_.find(key);
    if (it == values_.end() || it->second.value_type_ != ValueType::VALUE_TYPE_FLOAT)
      return false;

    *out = it->second.f_;
    return true;
  }
  bool get(const std::string &key, bool *out) const {
    auto it = values_.find(key);
    if (it == values_.end() || it->second.value_type_ != ValueType::VALUE_TYPE_BOOL)
      return false;

    *out = it->second.b_;
    return true;
  }
  bool get(const std::string &key, std::string *out) const {
    auto it = values_.find(key);
    if (it == values_.end() || it->second.value_type_ != ValueType::VALUE_TYPE_STRING)
      return false;

    *out = it->second.s_;
    return true;
  }
  bool has_value(const std::string &key) const { return values_.count(key) > 0; }
  void reset() {
    x = 0;
    y = 0;
    z = 0;
    state = 0;
    done = true;
    values_.clear();
  }
  std::unordered_map<std::string, Value> values_;
};

}  // namespace camera
}  // namespace esphome
