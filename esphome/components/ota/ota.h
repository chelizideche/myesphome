#pragma once

#include "esphome/core/defines.h"
#include "esphome/components/ota_base/ota_backend.h"

namespace esphome {
namespace ota {

// Import types from ota_base namespace for backward compatibility
using ota_base::OTABackend;
using ota_base::OTAComponent;
using ota_base::OTAResponseTypes;
using ota_base::OTAState;

// Re-export specific enum values for backward compatibility
// (in case external components use ota::OTA_STARTED, etc.)
static constexpr auto OTA_COMPLETED = ota_base::OTA_COMPLETED;
static constexpr auto OTA_STARTED = ota_base::OTA_STARTED;
static constexpr auto OTA_IN_PROGRESS = ota_base::OTA_IN_PROGRESS;
static constexpr auto OTA_ABORT = ota_base::OTA_ABORT;
static constexpr auto OTA_ERROR = ota_base::OTA_ERROR;

#ifdef USE_OTA_STATE_CALLBACK
using ota_base::OTAGlobalCallback;

// Deprecated: Use ota_base::get_global_ota_callback() instead
// Will be removed after 2025-12-30 (6 months from 2025-06-30)
[[deprecated("Use ota_base::get_global_ota_callback() instead")]] inline OTAGlobalCallback *get_global_ota_callback() {
  return ota_base::get_global_ota_callback();
}

// Deprecated: Use ota_base::register_ota_platform() instead
// Will be removed after 2025-12-30 (6 months from 2025-06-30)
[[deprecated("Use ota_base::register_ota_platform() instead")]] inline void register_ota_platform(
    OTAComponent *ota_caller) {
  ota_base::register_ota_platform(ota_caller);
}
#endif

}  // namespace ota
}  // namespace esphome