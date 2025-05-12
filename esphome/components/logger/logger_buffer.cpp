#include "logger_buffer.h"

#ifdef USE_ESPHOME_LOG_BUFFER

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger_buffer";

LogBuffer::LogBuffer(size_t total_buffer_size) {
  // Store the buffer size
  this->size_ = total_buffer_size;
  // Allocate memory for the ring buffer using ESPHome's RAM allocator
  RAMAllocator<uint8_t> allocator;
  this->storage_ = allocator.allocate(this->size_);
  // Create a static ring buffer with RINGBUF_TYPE_NOSPLIT for message integrity
  this->ring_buffer_ = xRingbufferCreateStatic(this->size_, RINGBUF_TYPE_NOSPLIT, this->storage_, &this->structure_);
}

LogBuffer::~LogBuffer() {
  if (this->ring_buffer_ != nullptr) {
    // Delete the ring buffer
    vRingbufferDelete(this->ring_buffer_);
    this->ring_buffer_ = nullptr;

    // Free the allocated memory
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->storage_, this->size_);
    this->storage_ = nullptr;
  }
}

bool LogBuffer::borrow_message_main_loop(LogMessage **message, const char **text, void **received_token) {
  // Check for valid output parameters
  if (message == nullptr || text == nullptr || received_token == nullptr) {
    return false;
  }

  // Retrieve item from ring buffer without blocking
  size_t item_size = 0;
  void *received_item = xRingbufferReceive(ring_buffer_, &item_size, 0);
  // Check if item received successfully
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

bool LogBuffer::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, TaskHandle_t task_handle,
                                         const char *format, va_list args) {
  // Stack allocation is required for thread safety
  // Buffer contains entire message (header + text + null terminator)
  uint8_t buffer[LOG_MSG_BUFFER_SIZE];

  // Set up the message header at the start of the buffer
  LogMessage *msg = reinterpret_cast<LogMessage *>(buffer);
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  // Use the provided task handle
  msg->task_handle = task_handle;

  // Format the message text directly after the header
  char *text_area = msg->text_data();
  // Format text with space for null terminator
  int ret = vsnprintf(text_area, LOG_MSG_SIZE_WITH_NULL, format, args);
  // vsnprintf handles truncation and adds a null terminator (not needed for storage but useful during formatting)

  // Check for formatting error or empty message
  if (ret <= 0) {
    return false;  // Formatting error or empty message
  }

  // Calculate actual text length (excluding null terminator)
  size_t text_length = (static_cast<size_t>(ret) > LOG_MSG_SIZE) ? LOG_MSG_SIZE : ret;

  // Remove trailing newlines immediately after formatting
  while (text_length > 0 && text_area[text_length - 1] == '\n') {
    text_length--;
  }

  // Set the final text length in the header
  msg->text_length = text_length;

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

  // Update counter to mark all messages as processed
  last_processed_counter_ = message_counter_.load(std::memory_order_relaxed);
}

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER
