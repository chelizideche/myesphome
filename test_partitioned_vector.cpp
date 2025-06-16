#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <string>

// Forward declare tests vector
struct Test {
  std::string name;
  void (*func)();
};
std::vector<Test> tests;

// Minimal test framework
#define TEST(name) \
  void test_##name(); \
  struct test_##name##_registrar { \
    test_##name##_registrar() { tests.push_back({#name, test_##name}); } \
  } test_##name##_instance; \
  void test_##name()

#define ASSERT(cond) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAILED: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
      exit(1); \
    } \
  } while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

// Mock classes matching ESPHome structure
const uint8_t COMPONENT_STATE_MASK = 0x07;
const uint8_t COMPONENT_STATE_LOOP = 0x02;
const uint8_t COMPONENT_STATE_LOOP_DONE = 0x04;
const uint8_t COMPONENT_STATE_FAILED = 0x03;

class Component {
 protected:
  uint8_t component_state_ = COMPONENT_STATE_LOOP;
  int id_;
  int loop_count_ = 0;

 public:
  Component(int id) : id_(id) {}
  virtual ~Component() = default;

  virtual void call() { loop_count_++; }

  int get_id() const { return id_; }
  int get_loop_count() const { return loop_count_; }
  uint8_t get_state() const { return component_state_ & COMPONENT_STATE_MASK; }

  void set_state(uint8_t state) { component_state_ = (component_state_ & ~COMPONENT_STATE_MASK) | state; }
};

class Application {
 public:
  std::vector<Component *> looping_components_;
  uint16_t looping_components_active_end_ = 0;
  uint16_t current_loop_index_ = 0;
  bool in_loop_ = false;

  void add_component(Component *c) {
    looping_components_.push_back(c);
    looping_components_active_end_ = looping_components_.size();
  }

  void loop() {
    in_loop_ = true;
    for (current_loop_index_ = 0; current_loop_index_ < looping_components_active_end_; current_loop_index_++) {
      looping_components_[current_loop_index_]->call();
    }
    in_loop_ = false;
  }

  void disable_component_loop(Component *component) {
    for (uint16_t i = 0; i < looping_components_active_end_; i++) {
      if (looping_components_[i] == component) {
        looping_components_active_end_--;
        if (i != looping_components_active_end_) {
          std::swap(looping_components_[i], looping_components_[looping_components_active_end_]);

          if (in_loop_ && i == current_loop_index_) {
            current_loop_index_--;
          }
        }
        return;
      }
    }
  }

  void enable_component_loop(Component *component) {
    const uint16_t size = looping_components_.size();
    for (uint16_t i = 0; i < size; i++) {
      if (looping_components_[i] == component) {
        if (i < looping_components_active_end_) {
          return;  // Already active
        }

        if (i != looping_components_active_end_) {
          std::swap(looping_components_[i], looping_components_[looping_components_active_end_]);
        }
        looping_components_active_end_++;
        return;
      }
    }
  }

  // Helper methods for testing
  std::vector<int> get_active_ids() const {
    std::vector<int> ids;
    for (uint16_t i = 0; i < looping_components_active_end_; i++) {
      ids.push_back(looping_components_[i]->get_id());
    }
    return ids;
  }

  bool is_component_active(Component *c) const {
    for (uint16_t i = 0; i < looping_components_active_end_; i++) {
      if (looping_components_[i] == c)
        return true;
    }
    return false;
  }
};

// Test basic functionality
TEST(basic_loop) {
  Application app;
  std::vector<std::unique_ptr<Component>> components;

  for (int i = 0; i < 5; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  app.loop();

  for (const auto &c : components) {
    ASSERT_EQ(c->get_loop_count(), 1);
  }
}

TEST(disable_component) {
  Application app;
  std::vector<std::unique_ptr<Component>> components;

  for (int i = 0; i < 5; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  // Disable component 2
  app.disable_component_loop(components[2].get());

  app.loop();

  // Components 0,1,3,4 should have been called
  ASSERT_EQ(components[0]->get_loop_count(), 1);
  ASSERT_EQ(components[1]->get_loop_count(), 1);
  ASSERT_EQ(components[2]->get_loop_count(), 0);  // Disabled
  ASSERT_EQ(components[3]->get_loop_count(), 1);
  ASSERT_EQ(components[4]->get_loop_count(), 1);

  // Verify partitioning
  ASSERT_EQ(app.looping_components_active_end_, 4);
  ASSERT(!app.is_component_active(components[2].get()));
}

TEST(enable_component) {
  Application app;
  std::vector<std::unique_ptr<Component>> components;

  for (int i = 0; i < 5; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  // Disable then re-enable
  app.disable_component_loop(components[2].get());
  app.enable_component_loop(components[2].get());

  app.loop();

  // All should have been called
  for (const auto &c : components) {
    ASSERT_EQ(c->get_loop_count(), 1);
  }

  ASSERT_EQ(app.looping_components_active_end_, 5);
}

TEST(multiple_disable_enable) {
  Application app;
  std::vector<std::unique_ptr<Component>> components;

  for (int i = 0; i < 10; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  // Disable multiple
  app.disable_component_loop(components[1].get());
  app.disable_component_loop(components[5].get());
  app.disable_component_loop(components[7].get());

  ASSERT_EQ(app.looping_components_active_end_, 7);

  app.loop();

  // Check counts
  int active_count = 0;
  for (const auto &c : components) {
    if (c->get_loop_count() == 1)
      active_count++;
  }
  ASSERT_EQ(active_count, 7);

  // Re-enable one
  app.enable_component_loop(components[5].get());
  ASSERT_EQ(app.looping_components_active_end_, 8);

  app.loop();

  ASSERT_EQ(components[5]->get_loop_count(), 1);
}

// Test reentrant behavior
class SelfDisablingComponent : public Component {
  Application *app_;

 public:
  SelfDisablingComponent(int id, Application *app) : Component(id), app_(app) {}

  void call() override {
    Component::call();
    if (loop_count_ == 2) {
      app_->disable_component_loop(this);
    }
  }
};

TEST(reentrant_disable) {
  Application app;
  std::vector<std::unique_ptr<Component>> components;

  // Add regular components
  for (int i = 0; i < 3; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  // Add self-disabling component
  auto self_disable = std::make_unique<SelfDisablingComponent>(3, &app);
  app.add_component(self_disable.get());

  // Add more regular components
  for (int i = 4; i < 6; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  // First loop - all active
  app.loop();
  ASSERT_EQ(app.looping_components_active_end_, 6);

  // Second loop - self-disabling component disables itself
  app.loop();
  ASSERT_EQ(app.looping_components_active_end_, 5);
  ASSERT_EQ(self_disable->get_loop_count(), 2);

  // Third loop - self-disabling component should not be called
  app.loop();
  ASSERT_EQ(self_disable->get_loop_count(), 2);  // Still 2
}

// Test edge cases
TEST(disable_already_disabled) {
  Application app;
  auto comp = std::make_unique<Component>(0);
  app.add_component(comp.get());

  app.disable_component_loop(comp.get());
  ASSERT_EQ(app.looping_components_active_end_, 0);

  // Disable again - should be no-op
  app.disable_component_loop(comp.get());
  ASSERT_EQ(app.looping_components_active_end_, 0);
}

TEST(enable_already_enabled) {
  Application app;
  auto comp = std::make_unique<Component>(0);
  app.add_component(comp.get());

  ASSERT_EQ(app.looping_components_active_end_, 1);

  // Enable again - should be no-op
  app.enable_component_loop(comp.get());
  ASSERT_EQ(app.looping_components_active_end_, 1);
}

TEST(disable_last_component) {
  Application app;
  auto comp = std::make_unique<Component>(0);
  app.add_component(comp.get());

  app.disable_component_loop(comp.get());
  ASSERT_EQ(app.looping_components_active_end_, 0);

  app.loop();  // Should not crash with empty active set
}

// Test that mimics real ESPHome component behavior
class MockSNTPComponent : public Component {
  Application *app_;
  bool time_synced_ = false;

 public:
  MockSNTPComponent(int id, Application *app) : Component(id), app_(app) {}

  void call() override {
    Component::call();

    // Simulate time sync after 3 calls
    if (loop_count_ >= 3 && !time_synced_) {
      time_synced_ = true;
      std::cout << "  SNTP: Time synced, disabling loop" << std::endl;
      set_state(COMPONENT_STATE_LOOP_DONE);
      app_->disable_component_loop(this);
    }
  }

  bool is_synced() const { return time_synced_; }
};

TEST(real_world_sntp) {
  Application app;

  // Regular components
  std::vector<std::unique_ptr<Component>> components;
  for (int i = 0; i < 5; i++) {
    components.push_back(std::make_unique<Component>(i));
    app.add_component(components.back().get());
  }

  // SNTP component
  auto sntp = std::make_unique<MockSNTPComponent>(5, &app);
  app.add_component(sntp.get());

  // Run 5 iterations
  for (int i = 0; i < 5; i++) {
    app.loop();
  }

  // SNTP should have disabled itself after 3 calls
  ASSERT_EQ(sntp->get_loop_count(), 3);
  ASSERT(sntp->is_synced());
  ASSERT_EQ(app.looping_components_active_end_, 5);  // SNTP removed

  // Regular components should have 5 calls each
  for (const auto &c : components) {
    ASSERT_EQ(c->get_loop_count(), 5);
  }
}

int main() {
  std::cout << "Running partitioned vector tests...\n" << std::endl;

  for (const auto &test : tests) {
    std::cout << "Running test: " << test.name << std::endl;
    test.func();
    std::cout << "  ✓ PASSED" << std::endl;
  }

  std::cout << "\nAll " << tests.size() << " tests passed!" << std::endl;
  return 0;
}