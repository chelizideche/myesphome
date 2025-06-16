#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstddef>

namespace esphome {
namespace esp32_ble {

// Lock-free SPSC queue that stores indices instead of pointers
// This allows us to use a pre-allocated pool of objects
template<size_t SIZE> class LockFreeIndexQueue {
 public:
  static constexpr size_t INVALID_INDEX = SIZE_MAX;

  LockFreeIndexQueue() : head_(0), tail_(0), dropped_count_(0) {
    // Initialize all slots to invalid
    for (size_t i = 0; i < SIZE; i++) {
      buffer_[i] = INVALID_INDEX;
    }
  }

  bool push(size_t index) {
    if (index == INVALID_INDEX)
      return false;

    size_t current_tail = tail_.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % SIZE;

    if (next_tail == head_.load(std::memory_order_acquire)) {
      // Buffer full
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    buffer_[current_tail] = index;
    tail_.store(next_tail, std::memory_order_release);
    return true;
  }

  size_t pop() {
    size_t current_head = head_.load(std::memory_order_relaxed);

    if (current_head == tail_.load(std::memory_order_acquire)) {
      return INVALID_INDEX;  // Empty
    }

    size_t index = buffer_[current_head];
    head_.store((current_head + 1) % SIZE, std::memory_order_release);
    return index;
  }

  size_t size() const {
    size_t tail = tail_.load(std::memory_order_acquire);
    size_t head = head_.load(std::memory_order_acquire);
    return (tail - head + SIZE) % SIZE;
  }

  size_t get_and_reset_dropped_count() { return dropped_count_.exchange(0, std::memory_order_relaxed); }

  void increment_dropped_count() { dropped_count_.fetch_add(1, std::memory_order_relaxed); }

  bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }

  bool full() const {
    size_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) % SIZE;
    return next_tail == head_.load(std::memory_order_acquire);
  }

 protected:
  size_t buffer_[SIZE];
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;
  std::atomic<size_t> dropped_count_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif