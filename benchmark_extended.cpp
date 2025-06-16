#include <iostream>
#include <vector>
#include <list>
#include <chrono>
#include <memory>
#include <iomanip>

class Component {
 public:
  Component(int id) : id_(id) {}

  void call() {
    // Minimal work to highlight iteration overhead
    volatile int x = id_;
    x++;
  }

  bool should_skip_loop() const { return skip_; }
  void set_skip(bool skip) { skip_ = skip; }

 private:
  int id_;
  bool skip_ = false;
  char padding_[119];  // Total size ~128 bytes
};

int main() {
  const int num_components = 40;
  const int iterations = 1000000;  // 1 million iterations

  std::cout << "=== Extended Performance Test ===" << std::endl;
  std::cout << "Components: " << num_components << std::endl;
  std::cout << "Iterations: " << iterations << std::endl;
  std::cout << "Testing overhead of flag checking vs list iteration\n" << std::endl;

  // Create components
  std::vector<std::unique_ptr<Component>> owned;
  std::vector<Component *> components;
  for (int i = 0; i < num_components; i++) {
    owned.push_back(std::make_unique<Component>(i));
    components.push_back(owned.back().get());
  }

  // Test 1: All components active (best case for both)
  {
    std::cout << "--- Test 1: All components active ---" << std::endl;

    // Vector test
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iterations; iter++) {
      for (auto *comp : components) {
        if (!comp->should_skip_loop()) {
          comp->call();
        }
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto vector_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // List test
    std::list<Component *> list_components(components.begin(), components.end());
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iterations; iter++) {
      for (auto *comp : list_components) {
        comp->call();
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto list_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Vector: " << vector_duration.count() << " µs" << std::endl;
    std::cout << "List: " << list_duration.count() << " µs" << std::endl;
    std::cout << "List is " << std::fixed << std::setprecision(1)
              << (list_duration.count() * 100.0 / vector_duration.count() - 100) << "% slower\n"
              << std::endl;
  }

  // Test 2: 25% components disabled (ESPHome scenario)
  {
    std::cout << "--- Test 2: 25% components disabled ---" << std::endl;

    // Disable 25% of components
    for (int i = 0; i < num_components / 4; i++) {
      components[i]->set_skip(true);
    }

    // Vector test
    auto start = std::chrono::high_resolution_clock::now();
    long long checks = 0, calls = 0;
    for (int iter = 0; iter < iterations; iter++) {
      for (auto *comp : components) {
        checks++;
        if (!comp->should_skip_loop()) {
          calls++;
          comp->call();
        }
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto vector_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // List test (with only active components)
    std::list<Component *> list_components;
    for (auto *comp : components) {
      if (!comp->should_skip_loop()) {
        list_components.push_back(comp);
      }
    }

    start = std::chrono::high_resolution_clock::now();
    long long list_calls = 0;
    for (int iter = 0; iter < iterations; iter++) {
      for (auto *comp : list_components) {
        list_calls++;
        comp->call();
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto list_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Vector: " << vector_duration.count() << " µs (" << checks << " checks, " << calls << " calls)"
              << std::endl;
    std::cout << "List: " << list_duration.count() << " µs (" << list_calls << " calls, no wasted checks)" << std::endl;
    std::cout << "Wasted work in vector: " << (checks - calls) << " flag checks" << std::endl;

    double overhead_percent = (vector_duration.count() - list_duration.count()) * 100.0 / list_duration.count();
    if (overhead_percent > 0) {
      std::cout << "Vector is " << std::fixed << std::setprecision(1) << overhead_percent
                << "% slower due to flag checking\n"
                << std::endl;
    } else {
      std::cout << "List is " << std::fixed << std::setprecision(1) << -overhead_percent << "% slower\n" << std::endl;
    }
  }

  // Test 3: Measure just the flag check overhead
  {
    std::cout << "--- Test 3: Pure flag check overhead ---" << std::endl;

    // Just flag checks, no calls
    auto start = std::chrono::high_resolution_clock::now();
    long long skipped = 0;
    for (int iter = 0; iter < iterations; iter++) {
      for (auto *comp : components) {
        if (comp->should_skip_loop()) {
          skipped++;
        }
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto check_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Time for " << (iterations * num_components) << " flag checks: " << check_duration.count() << " µs"
              << std::endl;
    std::cout << "Average per flag check: " << (check_duration.count() * 1000.0 / (iterations * num_components))
              << " ns" << std::endl;
    std::cout << "Checks that would skip work: " << skipped << std::endl;
  }

  return 0;
}