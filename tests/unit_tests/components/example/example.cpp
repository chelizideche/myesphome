#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Include here any esphome component:
// Make sure that in the yaml file you are adding a dummy configuration
// so that the component is actually pulled in!
#include "esphome/components/uart/uart_component.h"

// Now you can instantiate the component manually and write Google Tests:
TEST(ExampleTest, test1) {
  EXPECT_EQ(1, 1);
  EXPECT_NE(0, 1);
}
