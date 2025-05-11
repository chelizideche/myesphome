#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESPHOME_LOG_BUFFER

#include <cstddef>
#include <cstring>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

namespace esphome {
namespace logger {

// Maximum size for large messages (header size will be added to this)
constexpr size_t MAX_MESSAGE_TEXT_SIZE = 120;
// Minimum useful message text size
constexpr size_t MIN_USEFUL_MESSAGE_SIZE = 52;

class LogBuffer {
 public:
  // Structure for a log message header (text data follows immediately after)
  struct LogMessage {
    const char *tag;          // We store the pointer, assuming tags are static
    const char *thread_name;  // Thread name pointer (or nullptr for main thread)
    uint16_t text_length;     // Length of the message text (up to ~64KB)
    uint16_t line;            // Source code line number
    uint8_t level;            // Log level (0-7)

    // Helper to get pointer to text data following this header
    char *text_data() { return reinterpret_cast<char *>(this) + sizeof(LogMessage); }

    // Helper to get const pointer to text data following this header
    const char *text_data() const { return reinterpret_cast<const char *>(this) + sizeof(LogMessage); }

    // Get total size including header and text (plus null terminator)
    size_t total_size() const { return sizeof(LogMessage) + text_length + 1; }
  };

  // Constructor that takes a total buffer size
  explicit LogBuffer(size_t total_buffer_size);
  ~LogBuffer();

  // Attempt to reserve space in the ring buffer for a new message
  // Returns buffer to write text to, or nullptr if the buffer is full
  // Also returns the acquired item as token to make prepare/commit thread-safe
  char *prepare_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name, size_t &capacity,
                        void **message_token);

  // Commit the message with the given text length and token
  // Must be called after a successful prepare_message
  void commit_message(size_t text_length, void *message_token);

  // Borrow the next message from the ring buffer along with its text
  // Returns the message metadata and the message text through the out parameters
  // Returns false if the buffer is empty
  bool borrow_message(LogMessage **message, const char **text);

  // Release a previously borrowed message
  // Must be called after processing a message returned by borrow_message
  void release_message();

  // Cancel a prepared message without committing it
  // Call this if you want to abort a message preparation without committing
  void cancel_prepare(void *message_token);

 private:
  RingbufHandle_t ring_buffer_{nullptr};  // FreeRTOS ring buffer handle

  // For tracking received items in borrow/release operations
  void *received_item_{nullptr};  // Pointer to currently received item (for borrow/release)

  // Return total message size needed for given text length
  inline size_t message_size_for(size_t text_length) const {
    return sizeof(LogMessage) + text_length + 1;  // +1 for null terminator
  }
};

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER