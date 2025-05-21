#include "json_util.h"
#include "esphome/core/log.h"

#include <ArduinoJson/Memory/Allocator.hpp>

namespace esphome {
namespace json {

static const char *const TAG = "json";

// Build an allocator for the JSON Library using the RAMAllocator class
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return this->allocator_.allocate(size); }

  void deallocate(void *pointer) override {
    // RAMAllocator requires passing the size of the allocated space which don't know, so use free directly
    free(pointer);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  }

  void *reallocate(void *ptr, size_t new_size) override {
    return this->allocator_.reallocate(static_cast<uint8_t *>(ptr), new_size);
  }

 protected:
  RAMAllocator<uint8_t> allocator_{RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE)};
};

std::string build_json(const json_build_t &f) {
  // Here we are allocating up to 5kb of memory,
  // with the heap size minus 2kb to be safe if less than 5kb
  // as we can not have a true dynamic sized document.
  // The excess memory is freed below with `shrinkToFit()`
  while (true) {
    auto doc_allocator = SpiRamAllocator();
    JsonDocument json_document(&doc_allocator);
    if (json_document.overflowed()) {
      ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
      return "{}";
    }
    JsonObject root = json_document.to<JsonObject>();
    f(root);
    if (json_document.overflowed()) {
      ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
      return "{}";
    }
    std::string output;
    serializeJson(json_document, output);
    return output;
  }
}

bool parse_json(const std::string &data, const json_parse_t &f) {
  // Here we are allocating 1.5 times the data size,
  // with the heap size minus 2kb to be safe if less than that
  // as we can not have a true dynamic sized document.
  // The excess memory is freed below with `shrinkToFit()`
  while (true) {
    auto doc_allocator = SpiRamAllocator();
    JsonDocument json_document(&doc_allocator);
    if (json_document.overflowed()) {
      ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
      return false;
    }
    DeserializationError err = deserializeJson(json_document, data);

    JsonObject root = json_document.as<JsonObject>();

    if (err == DeserializationError::Ok) {
      return f(root);
    } else if (err == DeserializationError::NoMemory) {
      ESP_LOGE(TAG, "Can not allocate more memory for deserialization. Consider making source string smaller");
      return false;
    } else {
      ESP_LOGE(TAG, "JSON parse error: %s", err.c_str());
      return false;
    }
  };
  return false;
}

}  // namespace json
}  // namespace esphome
