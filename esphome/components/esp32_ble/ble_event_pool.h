#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstddef>
#include "ble_event.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esp32_ble {

// BLE Event Pool - On-demand pool of BLEEvent objects to avoid heap fragmentation
// Events are allocated on first use and reused thereafter, growing to peak usage
template<size_t SIZE> class BLEEventPool {
 public:
  BLEEventPool() {
    // Initialize all slots as unallocated
    for (size_t i = 0; i < SIZE; i++) {
      this->events_[i] = nullptr;
    }

    // Initialize the free list - all indices are initially free
    for (uint8_t i = 0; i < SIZE - 1; i++) {
      this->next_free_[i] = i + 1;
    }
    this->next_free_[SIZE - 1] = INVALID_INDEX;

    this->free_head_.store(0, std::memory_order_relaxed);
    this->allocated_count_.store(0, std::memory_order_relaxed);
    this->total_created_.store(0, std::memory_order_relaxed);
  }

  ~BLEEventPool() {
    // Delete any events that were created
    for (size_t i = 0; i < SIZE; i++) {
      if (this->events_[i] != nullptr) {
        delete this->events_[i];
      }
    }
  }

  // Allocate an event slot and return its index
  // Returns INVALID_INDEX if pool is full
  size_t allocate() {
    while (true) {
      uint8_t head = this->free_head_.load(std::memory_order_acquire);

      if (head == INVALID_INDEX) {
        // Pool is full
        return INVALID_INDEX;
      }

      uint8_t next = this->next_free_[head];

      // Try to update the free list head
      if (this->free_head_.compare_exchange_weak(head, next, std::memory_order_release, std::memory_order_acquire)) {
        this->allocated_count_.fetch_add(1, std::memory_order_relaxed);
        return head;
      }
      // CAS failed, retry
    }
  }

  // Deallocate an event slot by index
  void deallocate(size_t index) {
    if (index >= SIZE) {
      return;  // Invalid index
    }

    // No destructor call - events are reused
    // The event's reset methods handle cleanup when switching types

    while (true) {
      uint8_t head = this->free_head_.load(std::memory_order_acquire);
      this->next_free_[index] = head;

      // Try to add this index back to the free list
      if (this->free_head_.compare_exchange_weak(head, static_cast<uint8_t>(index), std::memory_order_release,
                                                 std::memory_order_acquire)) {
        this->allocated_count_.fetch_sub(1, std::memory_order_relaxed);
        return;
      }
      // CAS failed, retry
    }
  }

  // Get event by index, creating it if needed
  BLEEvent *get(size_t index) {
    if (index >= SIZE) {
      return nullptr;
    }

    // Create event on first access (warm-up)
    if (this->events_[index] == nullptr) {
      // Use internal RAM for better performance
      RAMAllocator<BLEEvent> allocator(RAMAllocator<BLEEvent>::ALLOC_INTERNAL);
      BLEEvent *event = allocator.allocate(1);

      if (event == nullptr) {
        // Fall back to regular allocation
        event = new BLEEvent();
      } else {
        // Placement new to construct the object
        new (event) BLEEvent();
      }

      this->events_[index] = event;
      this->total_created_.fetch_add(1, std::memory_order_relaxed);
    }

    return this->events_[index];
  }

  // Get number of allocated events
  size_t get_allocated_count() const { return this->allocated_count_.load(std::memory_order_relaxed); }

  // Get total number of events created (high water mark)
  size_t get_total_created() const { return this->total_created_.load(std::memory_order_relaxed); }

  static constexpr uint8_t INVALID_INDEX = 0xFF;  // 255, which is > MAX_BLE_QUEUE_SIZE (64)

 private:
  BLEEvent *events_[SIZE];                // Array of pointers, allocated on demand
  uint8_t next_free_[SIZE];               // Next free index for each slot
  std::atomic<uint8_t> free_head_;        // Head of the free list
  std::atomic<uint8_t> allocated_count_;  // Number of currently allocated events
  std::atomic<uint8_t> total_created_;    // Total events created (high water mark)
};

}  // namespace esp32_ble
}  // namespace esphome

#endif