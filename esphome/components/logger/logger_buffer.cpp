#include "logger_buffer.h"

#ifdef USE_ESPHOME_LOG_BUFFER

#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger_buffer";

LogBuffer::LogBuffer(size_t total_buffer_size) {
  // Ensure minimum buffer size for reasonable operation
  if (total_buffer_size < 512) {
    total_buffer_size = 512;  // 512 bytes minimum
  }

  // Create a byte buffer using xRingbufferCreate which handles memory allocation
  ring_buffer_ = xRingbufferCreate(total_buffer_size, RINGBUF_TYPE_BYTEBUF);

  if (ring_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create ring buffer");
    return;
  }

  // Initialize tracking pointers
  acquired_item_ = nullptr;
  received_item_ = nullptr;
}

LogBuffer::~LogBuffer() {
  if (ring_buffer_ != nullptr) {
    // Check if there are any unreleased items and release them
    if (acquired_item_ != nullptr) {
      xRingbufferReturnItem(ring_buffer_, acquired_item_);
      acquired_item_ = nullptr;
    }

    if (received_item_ != nullptr) {
      xRingbufferReturnItem(ring_buffer_, received_item_);
      received_item_ = nullptr;
    }

    // Delete the ring buffer (frees the memory automatically)
    vRingbufferDelete(ring_buffer_);
    ring_buffer_ = nullptr;
  }
}

char *LogBuffer::prepare_message(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                 size_t &capacity) {
  // Calculate minimum size needed for a usable message
  size_t min_size = message_size_for(MIN_USEFUL_MESSAGE_SIZE);

  // Try to acquire space in the ring buffer for a new message
  size_t item_size = 0;
  acquired_item_ = xRingbufferSendAcquire(ring_buffer_, &item_size, 0);

  if (acquired_item_ == nullptr || item_size < min_size) {
    // Not enough space, we'll let the consumer handle discarding old messages
    if (acquired_item_ != nullptr) {
      xRingbufferReturnItem(ring_buffer_, acquired_item_);
      acquired_item_ = nullptr;
    }

    capacity = 0;
    return nullptr;
  }

  // We have successfully acquired space in the ring buffer
  // Set up the message header at the start of the acquired space
  LogMessage *msg = static_cast<LogMessage *>(acquired_item_);
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

  // Return a pointer to where the text should be written
  return msg->text_data();
}

void LogBuffer::commit_message(size_t text_length) {
  // Check if we have an acquired item to commit
  if (acquired_item_ == nullptr || text_length == 0) {
    // Nothing to commit or zero text length, cancel the preparation
    cancel_prepare();
    return;
  }

  // Get the message header
  LogMessage *msg = static_cast<LogMessage *>(acquired_item_);

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
  xRingbufferSendComplete(ring_buffer_, acquired_item_);

  // Reset the acquired item pointer
  acquired_item_ = nullptr;
}

bool LogBuffer::borrow_message(LogMessage **message, const char **text) {
  // Check for valid output parameters
  if (message == nullptr || text == nullptr) {
    return false;
  }

  // Make sure we don't have a previously unreleased message
  if (received_item_ != nullptr) {
    release_message();
  }

  // Try to receive an item from the ring buffer
  size_t item_size = 0;
  received_item_ = xRingbufferReceive(ring_buffer_, &item_size, 0);

  if (received_item_ == nullptr || item_size < sizeof(LogMessage)) {
    // No message available or item too small to be valid
    if (received_item_ != nullptr) {
      xRingbufferReturnItem(ring_buffer_, received_item_);
      received_item_ = nullptr;
    }
    return false;
  }

  // Cast to LogMessage
  LogMessage *msg = static_cast<LogMessage *>(received_item_);

  // Validate the message size
  if (item_size < msg->total_size()) {
    // Message is truncated or invalid
    xRingbufferReturnItem(ring_buffer_, received_item_);
    received_item_ = nullptr;
    return false;
  }

  // Set the output parameters
  *message = msg;
  *text = msg->text_data();

  return true;
}

void LogBuffer::release_message() {
  // Check if there's a received item to release
  if (received_item_ != nullptr) {
    // Return the item to the ring buffer
    xRingbufferReturnItem(ring_buffer_, received_item_);
    received_item_ = nullptr;
  }
}

void LogBuffer::cancel_prepare() {
  // Check if we have an acquired item to cancel
  if (acquired_item_ != nullptr) {
    // Return the acquired item without committing
    xRingbufferReturnItem(ring_buffer_, acquired_item_);
    acquired_item_ = nullptr;
  }
}

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_LOG_BUFFER
