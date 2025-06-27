#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstddef>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/*
 * Lock-free queue for single-producer single-consumer scenarios.
 * This allows one thread to push items and another to pop them without
 * blocking each other.
 *
 * This is a Single-Producer Single-Consumer (SPSC) lock-free ring buffer.
 * Common use cases:
 * - BLE events: BLE task produces, main loop consumes
 * - MQTT messages: main task produces, MQTT thread consumes
 */

namespace esphome {

template<class T, uint8_t SIZE> class LockFreeQueue {
 public:
  LockFreeQueue() : head_(0), tail_(0), dropped_count_(0), task_to_notify_(nullptr) {}

  bool push(T *element) {
    if (element == nullptr)
      return false;

    uint8_t current_tail = tail_.load(std::memory_order_relaxed);
    uint8_t next_tail = (current_tail + 1) % SIZE;

    if (next_tail == head_.load(std::memory_order_acquire)) {
      // Buffer full
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    buffer_[current_tail] = element;
    tail_.store(next_tail, std::memory_order_release);

    // Notify the consumer task if set (optional)
    if (task_to_notify_ != nullptr) {
      xTaskNotifyGive(task_to_notify_);
    }

    return true;
  }

  T *pop() {
    uint8_t current_head = head_.load(std::memory_order_relaxed);

    if (current_head == tail_.load(std::memory_order_acquire)) {
      return nullptr;  // Empty
    }

    T *element = buffer_[current_head];
    head_.store((current_head + 1) % SIZE, std::memory_order_release);
    return element;
  }

  size_t size() const {
    uint8_t tail = tail_.load(std::memory_order_acquire);
    uint8_t head = head_.load(std::memory_order_acquire);
    return (tail - head + SIZE) % SIZE;
  }

  uint16_t get_and_reset_dropped_count() { return dropped_count_.exchange(0, std::memory_order_relaxed); }

  void increment_dropped_count() { dropped_count_.fetch_add(1, std::memory_order_relaxed); }

  bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }

  bool full() const {
    uint8_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) % SIZE;
    return next_tail == head_.load(std::memory_order_acquire);
  }

  void set_task_to_notify(TaskHandle_t task) { task_to_notify_ = task; }

 protected:
  T *buffer_[SIZE];
  // Atomic: written by producer (push/increment), read+reset by consumer (get_and_reset)
  std::atomic<uint16_t> dropped_count_;  // 65535 max - more than enough for drop tracking
  // Atomic: written by consumer (pop), read by producer (push) to check if full
  std::atomic<uint8_t> head_;
  // Atomic: written by producer (push), read by consumer (pop) to check if empty
  std::atomic<uint8_t> tail_;
  // Task handle for notification (optional)
  TaskHandle_t task_to_notify_;
};

}  // namespace esphome

#endif