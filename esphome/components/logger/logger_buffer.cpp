#include "logger_buffer.h"

#ifdef USE_ESPHOME_LOG_BUFFER

#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger_buffer";

LogBuffer::LogBuffer(size_t total_buffer_size) {
  // Create a no-split buffer for acquire/complete operations
  ring_buffer_ = xRingbufferCreate(total_buffer_size, RINGBUF_TYPE_NOSPLIT);
}

LogBuffer::~LogBuffer() {
  // Delete the ring buffer (frees the memory automatically)
  vRingbufferDelete(ring_buffer_);
  ring_buffer_ = nullptr;
}

char *LogBuffer::prepare_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                 size_t &capacity, void **message_token) {
  // Calculate minimum size needed for a usable message
  size_t min_size = message_size_for(MIN_USEFUL_MESSAGE_SIZE);

  // Try to acquire space in the ring buffer for a new message
  void *acquired_item = nullptr;

  // Request space for a message of adequate size
  BaseType_t result = xRingbufferSendAcquire(ring_buffer_, &acquired_item, min_size, 0);
  if (result != pdTRUE || acquired_item == nullptr) {
    // Failed to acquire space in the ring buffer
    capacity = 0;
    *message_token = nullptr;
    return nullptr;
  }

  // We successfully acquired space for our message
  size_t item_size = min_size;

  // We have successfully acquired space in the ring buffer
  // Set up the message header at the start of the acquired space
  LogMessage *msg = static_cast<LogMessage *>(acquired_item);
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  msg->thread_name = thread_name;
  msg->text_length = 0;  // Will be filled in commit

  // Calculate available capacity for text
  capacity = item_size - sizeof(LogMessage) - 1;  // -1 for null terminator

  // Cap capacity to a reasonable maximum
  if (capacity > MAX_MESSAGE_TEXT_SIZE) {
    capacity = MAX_MESSAGE_TEXT_SIZE;
  }

  // Store the acquired item pointer in the token for later use in commit
  *message_token = acquired_item;

  // Return a pointer to where the text should be written
  return msg->text_data();
}

void LogBuffer::commit_message(size_t text_length, void *message_token) {
  // Update commit operation counters
  commit_attempts_++;

  // Check if we have a valid message token to commit
  if (message_token == nullptr || text_length == 0) {
    // Nothing to commit or zero text length, cancel the preparation
    if (message_token != nullptr) {
      release_message(message_token);
    }
    return;
  }

  // Get the message header from the token
  LogMessage *msg = static_cast<LogMessage *>(message_token);

  // Calculate available space for text (excluding header and null terminator)
  size_t max_allowed_text = MAX_MESSAGE_TEXT_SIZE;

  // Limit text length if necessary
  if (text_length > max_allowed_text) {
    text_length = max_allowed_text;
  }

  // Set the text length in the message header
  msg->text_length = text_length;

  // Add null terminator
  msg->text_data()[text_length] = '\0';

  // Commit the message to the ring buffer
  BaseType_t result = xRingbufferSendComplete(ring_buffer_, message_token);
  if (result == pdTRUE) {
    commit_success_++;
  }

  // Save counters for main loop to report - no direct console output here
}

bool LogBuffer::borrow_message(LogMessage **message, const char **text, void **received_token) {
  // Check for valid output parameters
  if (message == nullptr || text == nullptr || received_token == nullptr) {
    return false;
  }

  // Update borrow counters
  borrow_attempts_++;

  // Try to receive an item from the ring buffer
  size_t item_size = 0;
  void *received_item = xRingbufferReceive(ring_buffer_, &item_size, 0);
  // xRingbufferReceive returns NULL if no items are available, otherwise returns a pointer to the received item

  if (received_item == nullptr) {
    borrow_items_null_++;
    return false;
  }

  if (item_size < sizeof(LogMessage)) {
    // Item too small to be valid
    borrow_items_invalid_++;
    vRingbufferReturnItem(ring_buffer_, received_item);
    return false;
  }

  // Cast to LogMessage
  LogMessage *msg = static_cast<LogMessage *>(received_item);

  // Validate the message size
  if (item_size < msg->total_size()) {
    // Message is truncated or invalid
    borrow_items_invalid_++;
    vRingbufferReturnItem(ring_buffer_, received_item);
    return false;
  }

  // Set the output parameters
  *message = msg;
  *text = msg->text_data();
  *received_token = received_item;

  // Update success counter
  borrow_success_++;
  return true;
}

void LogBuffer::release_message(void *token) {
  // Check if there's a valid token to release
  if (token != nullptr) {
    // Return the item to the ring buffer
    vRingbufferReturnItem(ring_buffer_, token);
  }
}

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER
