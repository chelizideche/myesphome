#pragma once

#include "esphome/core/component.h"
#include <atomic>

namespace esphome {
namespace defer_stress_component {

class DeferStressComponent : public Component {
 public:
  void setup() override;
  void run_multi_thread_test();

  // Getters for test status
  int get_total_defers();
  int get_executed_defers();
  bool is_test_complete();
  int get_expected_total();

 private:
  std::atomic<int> total_defers_{0};
  std::atomic<int> executed_defers_{0};
  bool test_complete_{false};
  int expected_total_{0};
};

}  // namespace defer_stress_component
}  // namespace esphome