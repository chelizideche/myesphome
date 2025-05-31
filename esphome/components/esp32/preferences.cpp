#ifdef USE_ESP32

#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <nvs_flash.h>
#include <cstring>
#include <cinttypes>
#include <vector>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32.preferences";

// Stack size for the NVS sync task - 2KB should be enough for NVS operations
static const size_t NVS_SYNC_TASK_STACK_SIZE = 2048;

struct NVSData {
  std::string key;
  std::vector<uint8_t> data;
};

static std::vector<NVSData> s_pending_save;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<bool> s_sync_task_running{false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

struct SyncTaskData {
  std::vector<NVSData> pending_save;
};

class ESP32PreferenceBackend : public ESPPreferenceBackend {
 public:
  std::string key;
  uint32_t nvs_handle;
  bool save(const uint8_t *data, size_t len) override {
    // try find in pending saves and update that
    for (auto &obj : s_pending_save) {
      if (obj.key == key) {
        obj.data.assign(data, data + len);
        return true;
      }
    }
    NVSData save{};
    save.key = key;
    save.data.assign(data, data + len);
    s_pending_save.emplace_back(save);
    ESP_LOGVV(TAG, "s_pending_save: key: %s, len: %d", key.c_str(), len);
    return true;
  }
  bool load(uint8_t *data, size_t len) override {
    // try find in pending saves and load from that
    for (auto &obj : s_pending_save) {
      if (obj.key == key) {
        if (obj.data.size() != len) {
          // size mismatch
          return false;
        }
        memcpy(data, obj.data.data(), len);
        return true;
      }
    }

    size_t actual_len;
    esp_err_t err = nvs_get_blob(nvs_handle, key.c_str(), nullptr, &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key.c_str(), esp_err_to_name(err));
      return false;
    }
    if (actual_len != len) {
      ESP_LOGVV(TAG, "NVS length does not match (%u!=%u)", actual_len, len);
      return false;
    }
    err = nvs_get_blob(nvs_handle, key.c_str(), data, &len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key.c_str(), esp_err_to_name(err));
      return false;
    } else {
      ESP_LOGVV(TAG, "nvs_get_blob: key: %s, len: %d", key.c_str(), len);
    }
    return true;
  }
};

// Forward declaration
static void sync_task(void *param);

class ESP32Preferences : public ESPPreferences {
 public:
  uint32_t nvs_handle;

  void open() {
    nvs_flash_init();
    esp_err_t err = nvs_open("esphome", NVS_READWRITE, &nvs_handle);
    if (err == 0)
      return;

    ESP_LOGW(TAG, "nvs_open failed: %s - erasing NVS...", esp_err_to_name(err));
    nvs_flash_deinit();
    nvs_flash_erase();
    nvs_flash_init();

    err = nvs_open("esphome", NVS_READWRITE, &nvs_handle);
    if (err != 0) {
      nvs_handle = 0;
    }
  }

 public:
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new ESP32PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->nvs_handle = nvs_handle;

    uint32_t keyval = type;
    pref->key = str_sprintf("%" PRIu32, keyval);

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    if (s_pending_save.empty())
      return true;

    // Try to mark sync task as running using atomic compare-exchange
    bool expected = false;
    if (!s_sync_task_running.compare_exchange_strong(expected, true)) {
      ESP_LOGD(TAG, "NVS sync task already running, skipping");
      return true;
    }

    // Create task data and move pending saves to it
    auto *task_data = new SyncTaskData();
    task_data->pending_save = std::move(s_pending_save);
    s_pending_save.clear();

    // Create the sync task
    BaseType_t task_created = xTaskCreate(sync_task,                 // Task function
                                          "nvs_sync",                // Task name
                                          NVS_SYNC_TASK_STACK_SIZE,  // Stack size in bytes
                                          task_data,                 // Task parameter
                                          1,                         // Priority (low)
                                          nullptr                    // Task handle (not needed)
    );

    if (task_created != pdPASS) {
      ESP_LOGE(TAG, "Failed to create NVS sync task");
      // Restore pending saves on failure
      s_pending_save = std::move(task_data->pending_save);
      s_sync_task_running.store(false);
      delete task_data;
      return false;
    }

    // Task is now running in the background - return immediately
    // The task will clean up its own resources when done
    return true;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Cleaning up preferences in flash...");
    s_pending_save.clear();

    nvs_flash_deinit();
    nvs_flash_erase();
    // Make the handle invalid to prevent any saves until restart
    nvs_handle = 0;
    return true;
  }
};

// Helper function to check if NVS data has changed
static bool is_nvs_changed(const uint32_t nvs_handle, const NVSData &to_save) {
  NVSData stored_data{};
  size_t actual_len;
  esp_err_t err = nvs_get_blob(nvs_handle, to_save.key.c_str(), nullptr, &actual_len);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", to_save.key.c_str(), esp_err_to_name(err));
    return true;
  }
  stored_data.data.resize(actual_len);
  err = nvs_get_blob(nvs_handle, to_save.key.c_str(), stored_data.data.data(), &actual_len);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", to_save.key.c_str(), esp_err_to_name(err));
    return true;
  }
  return to_save.data != stored_data.data;
}

// Sync task implementation
static void sync_task(void *param) {
  auto *task_data = static_cast<SyncTaskData *>(param);

  // Open a new NVS handle for this task
  nvs_handle_t task_nvs_handle;
  esp_err_t err = nvs_open("esphome", NVS_READWRITE, &task_nvs_handle);
  if (err != 0) {
    ESP_LOGE(TAG, "Failed to open NVS handle in sync task: %s", esp_err_to_name(err));
    delete task_data;
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGD(TAG, "Saving %d preferences to flash in background...", task_data->pending_save.size());

  int cached = 0, written = 0, failed = 0;
  esp_err_t last_err = ESP_OK;
  std::string last_key{};

  // Process all pending saves
  for (const auto &save : task_data->pending_save) {
    ESP_LOGVV(TAG, "Checking if NVS data %s has changed", save.key.c_str());
    if (is_nvs_changed(task_nvs_handle, save)) {
      esp_err_t err = nvs_set_blob(task_nvs_handle, save.key.c_str(), save.data.data(), save.data.size());
      ESP_LOGV(TAG, "sync: key: %s, len: %d", save.key.c_str(), save.data.size());
      if (err != 0) {
        ESP_LOGV(TAG, "nvs_set_blob('%s', len=%u) failed: %s", save.key.c_str(), save.data.size(),
                 esp_err_to_name(err));
        failed++;
        last_err = err;
        last_key = save.key;
        continue;
      }
      written++;
    } else {
      ESP_LOGV(TAG, "NVS data not changed skipping %s  len=%u", save.key.c_str(), save.data.size());
      cached++;
    }
  }

  ESP_LOGD(TAG, "Background save complete: %d cached, %d written, %d failed", cached, written, failed);

  if (failed > 0) {
    ESP_LOGE(TAG, "Error saving %d preferences to flash. Last error=%s for key=%s", failed, esp_err_to_name(last_err),
             last_key.c_str());
  }

  // Commit changes
  err = nvs_commit(task_nvs_handle);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_commit() failed: %s", esp_err_to_name(err));
  }

  // Close the NVS handle
  nvs_close(task_nvs_handle);

  // Clean up
  delete task_data;

  // Mark sync task as no longer running
  s_sync_task_running.store(false);

  // Delete this task
  vTaskDelete(nullptr);
}

void setup_preferences() {
  auto *prefs = new ESP32Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  prefs->open();
  global_preferences = prefs;
}

}  // namespace esp32

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_ESP32
