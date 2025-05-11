#include "logger_buffer.h"

#ifdef USE_ESPHOME_LOG_BUFFER

#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger_buffer";

LogBuffer::LogBuffer(size_t total_buffer_size) {
  // Create a byte buffer for message operations
  // The size is guaranteed to be at least 1024 bytes by the config validation
  ring_buffer_ = xRingbufferCreate(total_buffer_size, RINGBUF_TYPE_BYTES);
}

LogBuffer::~LogBuffer() {
  // Delete the ring buffer (frees the memory automatically)
  vRingbufferDelete(ring_buffer_);
  ring_buffer_ = nullptr;
}

char *LogBuffer::prepare_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                 size_t &capacity, void **message_token) {
  // Use the predefined buffer size that includes everything we need
  size_t max_size = LOG_MSG_BUFFER_SIZE;

  // Try to acquire space in the ring buffer for a new message
  void *acquired_item = nullptr;

  // Request space for maximum possible message size to ensure we have enough space
  BaseType_t result = xRingbufferSendAcquire(ring_buffer_, &acquired_item, max_size, 0);
  if (result != pdTRUE || acquired_item == nullptr) {
    // Failed to acquire space in the ring buffer
    capacity = 0;
    *message_token = nullptr;
    return nullptr;
  }

  // We successfully acquired space for our message - use full max size since NOSPLIT
  // ring buffer will allocate the entire requested size
  size_t item_size = max_size;

  // We have successfully acquired space in the ring buffer
  // Set up the message header at the start of the acquired space
  LogMessage *msg = static_cast<LogMessage *>(acquired_item);
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  msg->thread_name = thread_name;
  msg->text_length = 0;  // Will be filled in commit

  // Calculate available capacity for text
  capacity = item_size - LOG_MSG_STRUCT_SIZE;

  // Cap capacity to a reasonable maximum
  if (capacity > LOG_MSG_SIZE) {
    capacity = LOG_MSG_SIZE;
  }

  // Store the acquired item pointer in the token for later use in commit
  *message_token = acquired_item;

  // Return a pointer to where the text should be written
  return msg->text_data();
}

void LogBuffer::commit_message(size_t text_length, void *message_token) {
  // Check if we have a valid message token to commit
  if (message_token == nullptr || text_length == 0) {
    // Nothing to commit or zero text length, cancel the preparation
    if (message_token != nullptr) {
      cancel_message(message_token);
    }
    return;
  }

  // Get the message header from the token
  LogMessage *msg = static_cast<LogMessage *>(message_token);

  // Set the text length in the message header
  // No need to add null terminator as we track the length explicitly
  msg->text_length = text_length;

  // Commit the message to the ring buffer
  xRingbufferSendComplete(ring_buffer_, message_token);

  // Increment the message counter to notify the logger task
  message_counter_.fetch_add(1, std::memory_order_relaxed);
}

bool LogBuffer::borrow_message_main_loop(LogMessage **message, const char **text, void **received_token) {
  // Check for valid output parameters
  if (message == nullptr || text == nullptr || received_token == nullptr) {
    return false;
  }

  // Try to receive an item from the ring buffer - no timeout to avoid blocking
  size_t item_size = 0;
  void *received_item = xRingbufferReceive(ring_buffer_, &item_size, 0);
  // xRingbufferReceive returns NULL if no items are available, otherwise returns a pointer to the received item
  if (received_item == nullptr) {
    return false;
  }

  // Cast to LogMessage - we know the item is valid because we control the allocation
  LogMessage *msg = static_cast<LogMessage *>(received_item);
  *message = msg;
  *text = msg->text_data();
  *received_token = received_item;

  return true;
}

void LogBuffer::cancel_message(void *token) {
  // Check if there's a valid token to cancel
  if (token == nullptr) {
    return;  // Nothing to cancel
  }
  // Return the item to the ring buffer without committing
  vRingbufferReturnItem(ring_buffer_, token);
}

bool LogBuffer::send_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name, const char *format,
                             va_list args) {
  // Create a buffer for the entire message (header + text + null terminator)
  // Stack allocation is fine for this size (typically ~150 bytes total)
  uint8_t buffer[LOG_MSG_BUFFER_SIZE];

  // Set up the message header at the start of the buffer
  LogMessage *msg = reinterpret_cast<LogMessage *>(buffer);
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  msg->thread_name = thread_name;

  // Format the message text directly after the header
  char *text_area = msg->text_data();
  // Format into the text area using the size that includes space for null terminator
  int ret = vsnprintf(text_area, LOG_MSG_SIZE_WITH_NULL, format, args);
  // vsnprintf will truncate the string if needed, and always adds a null terminator
  // We don't need the null terminator in our final message, but let vsnprintf add it temporarily

  // Check for formatting error or empty message
  if (ret <= 0) {
    return false;  // Formatting error or empty message
  }

  // Calculate actual text length (handle truncation)
  // This doesn't include the null terminator
  size_t text_length = (static_cast<size_t>(ret) > LOG_MSG_SIZE) ? LOG_MSG_SIZE : ret;

  // Remove trailing newlines - text_length points to the position after the last character
  // so text_area[text_length-1] is the last character, not the null terminator
  while (text_length > 0 && text_area[text_length - 1] == '\n') {
    text_length--;
  }

  // If we have no text after processing, don't send the message
  if (text_length == 0) {
    return false;
  }

  // Set the final text length in the header
  msg->text_length = text_length;

  // No need to add null terminator as we track the length explicitly

  // Calculate total size to send (header + text)
  size_t total_size = sizeof(LogMessage) + text_length;

  // Send the message to the ring buffer
  BaseType_t result = xRingbufferSend(ring_buffer_, buffer, total_size, 0);

  // Check if sending failed
  if (result != pdTRUE) {
    return false;  // Failed to send the message
  }

  // Message sent successfully, increment the counter
  message_counter_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

void LogBuffer::release_message_main_loop(void *token) {
  // Check if there's a valid token to release
  if (token == nullptr) {
    return;  // Nothing to release
  }
  // Return the item to the ring buffer
  vRingbufferReturnItem(ring_buffer_, token);

  // Update the last processed counter to match the current message counter
  // This marks all messages as processed up to the current point
  last_processed_counter_ = message_counter_.load(std::memory_order_relaxed);
}

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER
