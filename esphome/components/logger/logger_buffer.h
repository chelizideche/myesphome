#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESPHOME_LOG_BUFFER

#include <cstddef>  // For size_t
#include <atomic>
#include <cstring>  // For memcpy
#include <memory>   // For unique_ptr

// Backport of std::make_unique from C++14
#if __cplusplus < 201402L
namespace std {
template<class T, class... Args>
typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type make_unique(Args &&...args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class T>
typename std::enable_if<std::is_array<T>::value, std::unique_ptr<T>>::type make_unique(std::size_t n) {
  typedef typename std::remove_extent<T>::type RT;
  return std::unique_ptr<T>(new RT[n]);
}
}  // namespace std
#endif

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
  char *prepare_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name, size_t &capacity);

  // Commit the message with the given text length
  // Must be called after a successful prepare_message
  void commit_message(size_t text_length);

  // Borrow the next message from the ring buffer along with its text
  // Returns the message metadata and the message text through the out parameters
  // Returns false if the buffer is empty
  bool borrow_message(LogMessage **message, const char **text);

  // Release a previously borrowed message
  // Must be called after processing a message returned by borrow_message
  void release_message();

  // Cancel a prepared message without committing it
  // Call this if you want to abort a message preparation without committing
  void cancel_prepare();

 private:
  char *buffer_;             // Single ring buffer for all messages and their text
  size_t buffer_size_;       // Total size of the buffer
  size_t max_message_size_;  // Maximum size a single message can occupy

  // Positions in the ring buffer
  LogMessage *read_pos_{nullptr};      // Position to read next message from
  LogMessage *write_pos_{nullptr};     // Position to write next message to
  LogMessage *prepared_pos_{nullptr};  // Position where message is being prepared

  // Using atomic operations for thread-safe lock-free operation
  std::atomic<bool> message_prepared_{false};  // Flag to prevent concurrent prepare operations
  std::atomic<size_t> read_index_{0};          // Current read position index
  std::atomic<size_t> write_index_{0};         // Current write position index

  // Helper method to release lock and return nullptr
  inline char *release_lock_and_return_null_() {
    message_prepared_.store(false, std::memory_order_release);
    return nullptr;
  }

  // Public method to release the prepared lock without committing
  inline void cancel_prepare() { message_prepared_.store(false, std::memory_order_release); }
};

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER
