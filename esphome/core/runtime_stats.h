#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {

static const char *const RUNTIME_TAG = "runtime";

class Component;  // Forward declaration

class ComponentRuntimeStats {
 public:
  ComponentRuntimeStats() : count_(0), total_time_ms_(0), max_time_ms_(0) {}

  void record_time(uint32_t duration_ms) {
    this->count_++;
    this->total_time_ms_ += duration_ms;

    if (duration_ms > this->max_time_ms_)
      this->max_time_ms_ = duration_ms;
  }

  void reset() {
    this->count_ = 0;
    this->total_time_ms_ = 0;
    this->max_time_ms_ = 0;
  }

  uint32_t get_count() const { return this->count_; }
  uint32_t get_total_time_ms() const { return this->total_time_ms_; }
  uint32_t get_max_time_ms() const { return this->max_time_ms_; }
  float get_avg_time_ms() const {
    return this->count_ > 0 ? this->total_time_ms_ / static_cast<float>(this->count_) : 0.0f;
  }

 protected:
  uint32_t count_;
  uint32_t total_time_ms_;
  uint32_t max_time_ms_;
};

// For sorting components by total run time
struct ComponentStatPair {
  std::string name;
  const ComponentRuntimeStats *stats;

  bool operator>(const ComponentStatPair &other) const {
    return stats->get_total_time_ms() > other.stats->get_total_time_ms();
  }
};

class RuntimeStatsCollector {
 public:
  RuntimeStatsCollector() : log_interval_(60000), next_log_time_(0), enabled_(true) {}

  void set_log_interval(uint32_t log_interval) { this->log_interval_ = log_interval; }
  uint32_t get_log_interval() const { return this->log_interval_; }

  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  bool is_enabled() const { return this->enabled_; }

  void record_component_time(Component *component, uint32_t duration_ms, uint32_t current_time);

 protected:
  void log_stats_() {
    ESP_LOGI(RUNTIME_TAG, "Component Runtime Statistics (over last %" PRIu32 "ms):", this->log_interval_);

    // First collect stats we want to display
    std::vector<ComponentStatPair> stats_to_display;

    for (const auto &it : this->component_stats_) {
      const ComponentRuntimeStats &stats = it.second;
      if (stats.get_count() > 0) {
        ComponentStatPair pair = {it.first, &stats};
        stats_to_display.push_back(pair);
      }
    }

    // Sort by total runtime (descending)
    std::sort(stats_to_display.begin(), stats_to_display.end(), std::greater<ComponentStatPair>());

    // Log top components by runtime
    for (const auto &it : stats_to_display) {
      const std::string &source = it.name;
      const ComponentRuntimeStats *stats = it.stats;

      ESP_LOGI(RUNTIME_TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms",
               source.c_str(), stats->get_count(), stats->get_avg_time_ms(), stats->get_max_time_ms(),
               stats->get_total_time_ms());
    }
  }

  void reset_stats_() {
    for (auto &it : this->component_stats_) {
      it.second.reset();
    }
  }

  std::map<std::string, ComponentRuntimeStats> component_stats_;
  uint32_t log_interval_;
  uint32_t next_log_time_;
  bool enabled_;
};

// Global instance for runtime stats collection
extern RuntimeStatsCollector runtime_stats;

}  // namespace esphome