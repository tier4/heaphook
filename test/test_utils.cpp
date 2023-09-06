
#include <gtest/gtest.h>

#include "heaphook/utils.hpp"

TEST(UtilsTest, IsPowerOf2Test) {
  EXPECT_EQ(is_power_of_2(0), false);
  EXPECT_EQ(is_power_of_2(1), true);
  EXPECT_EQ(is_power_of_2(2), true);
  EXPECT_EQ(is_power_of_2(3), false);
  EXPECT_EQ(is_power_of_2(4), true);
  EXPECT_EQ(is_power_of_2(5), false);
  EXPECT_EQ(is_power_of_2(6), false);
  EXPECT_EQ(is_power_of_2(7), false);
  EXPECT_EQ(is_power_of_2(8), true);
  EXPECT_EQ(is_power_of_2(9), false);
}

TEST(UtilsTest, IsValidAlignmentTest) {
  if (sizeof(void *) == 8) { // 64 bit mode
    EXPECT_EQ(is_valid_alignment(0), false);
    EXPECT_EQ(is_valid_alignment(1), false);
    EXPECT_EQ(is_valid_alignment(2), false);
    EXPECT_EQ(is_valid_alignment(3), false);
    EXPECT_EQ(is_valid_alignment(4), false);
    EXPECT_EQ(is_valid_alignment(5), false);
    EXPECT_EQ(is_valid_alignment(6), false);
    EXPECT_EQ(is_valid_alignment(7), false);
    EXPECT_EQ(is_valid_alignment(8), true);
    EXPECT_EQ(is_valid_alignment(9), false);
    EXPECT_EQ(is_valid_alignment(10), false);
    EXPECT_EQ(is_valid_alignment(16), true);
    EXPECT_EQ(is_valid_alignment(24), false); // is not power of 2
  } else { // 32 bit mode
    // not implemented
  }
}

TEST(UtilsTest, NextPowerOf2Test) {
  EXPECT_EQ(next_power_of_2(0), 1ull);
  EXPECT_EQ(next_power_of_2(1), 1ull);
  EXPECT_EQ(next_power_of_2(2), 2ull);
  EXPECT_EQ(next_power_of_2(3), 4ull);
  EXPECT_EQ(next_power_of_2(4), 4ull);
  EXPECT_EQ(next_power_of_2(5), 8ull);
  EXPECT_EQ(next_power_of_2(8), 8ull);
  EXPECT_EQ(next_power_of_2(9), 16ull);
  EXPECT_EQ(next_power_of_2(0x101), 0x200ull);
  EXPECT_EQ(next_power_of_2(0x7fffffffffffffffull), 0x8000000000000000);
  EXPECT_EQ(next_power_of_2(0xffffffffffffffffull), 0ull); // error
}