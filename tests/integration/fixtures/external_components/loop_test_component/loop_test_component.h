#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace loop_test_component {

static const char *const TAG = "loop_test_component";

class LoopTestComponent : public Component {
 public:
  void set_name(const std::string &name) { this->name_ = name; }
  void set_disable_after(int count) { this->disable_after_ = count; }
  void set_test_redundant_operations(bool test) { this->test_redundant_operations_ = test; }

  void setup() override { ESP_LOGI(TAG, "[%s] Setup called", this->name_.c_str()); }

  void loop() override {
    this->loop_count_++;
    ESP_LOGI(TAG, "[%s] Loop count: %d", this->name_.c_str(), this->loop_count_);

    // Test self-disable after specified count
    if (this->disable_after_ > 0 && this->loop_count_ == this->disable_after_) {
      ESP_LOGI(TAG, "[%s] Disabling self after %d loops", this->name_.c_str(), this->disable_after_);
      this->disable_loop();
    }

    // Test redundant operations
    if (this->test_redundant_operations_ && this->loop_count_ == 5) {
      if (this->name_ == "redundant_enable") {
        ESP_LOGI(TAG, "[%s] Testing enable when already enabled", this->name_.c_str());
        this->enable_loop();
      } else if (this->name_ == "redundant_disable") {
        ESP_LOGI(TAG, "[%s] Testing disable when will be disabled", this->name_.c_str());
        // We'll disable at count 10, but try to disable again at 5
        this->disable_loop();
        ESP_LOGI(TAG, "[%s] First disable complete", this->name_.c_str());
      }
    }
  }

  // Service methods for external control
  void service_enable() {
    ESP_LOGI(TAG, "[%s] Service enable called", this->name_.c_str());
    this->enable_loop();
  }

  void service_disable() {
    ESP_LOGI(TAG, "[%s] Service disable called", this->name_.c_str());
    this->disable_loop();
  }

  int get_loop_count() const { return this->loop_count_; }

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  std::string name_;
  int loop_count_{0};
  int disable_after_{0};
  bool test_redundant_operations_{false};
};

template<typename... Ts> class EnableAction : public Action<Ts...> {
 public:
  EnableAction(LoopTestComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->service_enable(); }

 protected:
  LoopTestComponent *parent_;
};

template<typename... Ts> class DisableAction : public Action<Ts...> {
 public:
  DisableAction(LoopTestComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->service_disable(); }

 protected:
  LoopTestComponent *parent_;
};

}  // namespace loop_test_component
}  // namespace esphome