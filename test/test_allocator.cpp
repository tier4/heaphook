#include <cstdlib>
#include <malloc.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <gtest/gtest.h>
#include <thread>

#include "heaphook/heaphook.hpp"
#include "heaphook/utils.hpp"

using namespace heaphook;

TEST(alloc_test, valid_size_test) {
  auto test = [](size_t size) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc(size));
    EXPECT_TRUE(ptr != nullptr);
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(size <= block_size);
    for (size_t i = 0; i < size; i++) {
      ptr[i] = 'A';
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };

  for (auto size: { 1, 15, 123, getpagesize(), 0x12345 }) {
    test(size);
  }
}

TEST(alloc_test, invalid_size_test) {
  auto test = [](size_t size) {
    void *ptr = GlobalAllocator::get_instance().alloc(size);
    EXPECT_TRUE(ptr == nullptr);
  };

  test(0x1000000000000); // 256 TB
  test(SIZE_MAX);
}

TEST(alloc_test, alignment_test) {
  auto test = [](size_t size, size_t alignment) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc(size, alignment));
    size_t addr = reinterpret_cast<size_t>(ptr);
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_TRUE(addr % alignment == 0u);
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(size <= block_size);
    for (size_t i = 0; i < size; i++) {
      ptr[i] = 'A';
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };
  
  for (auto size: { 1, 15, 123, getpagesize(), 0x12345 }) {
    test(size, sizeof(void *));
    test(size, getpagesize());
  }
}

TEST(get_block_size_test, write_test) {
  auto test = [](size_t size, size_t alignment) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc(size, alignment));
    EXPECT_TRUE(ptr != nullptr);
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(size <= block_size);
    for (size_t i = 0; i < block_size; i++) {
      ptr[i] = 'A';
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };

  for (auto size: { 1, 15, 123, getpagesize(), 0x12345 }) {
    test(size, 1);
    test(size, 64);
  }
}

TEST(alloc_zeroed_test, valid_size_test) {
  auto test = [](size_t size) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc_zeroed(size));
    EXPECT_TRUE(ptr != nullptr);
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(size <= block_size);
    for (size_t i = 0; i < size; i++) {
      EXPECT_TRUE(ptr[i] == 0);
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };

  for (auto size: { 1, 15, 123, getpagesize(), 0x12345 }) {
    test(size);
  }
}

TEST(alloc_zeroed_test, invalid_size_test) {
  auto test = [](size_t size) {
    void *ptr = GlobalAllocator::get_instance().alloc_zeroed(size);
    EXPECT_TRUE(ptr == nullptr);
  };

  test(0x1000000000000); // 256 TB
  test(SIZE_MAX);
}

TEST(realloc_test, alloc_to_realloc_test) {
  auto test = [](size_t old_size, size_t new_size) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc(old_size));
    EXPECT_TRUE(ptr != nullptr);
    for (size_t i = 0; i < old_size; i++) {
      ptr[i] = 'A';
    }
    ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().realloc(ptr, new_size));
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(new_size <= block_size);
    for (size_t i = 0; i < std::min(old_size, new_size); i++) {
      EXPECT_TRUE(ptr[i] == 'A');
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };

  test(1, 15);
  test(15, 123);
  test(123, getpagesize());
  test(15, 1);
  test(123, 15);
  test(getpagesize(), 123);
}

TEST(realloc_test, alloc_zeroed_to_realloc_test) {
  auto test = [](size_t old_size, size_t new_size) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc_zeroed(old_size));
    EXPECT_TRUE(ptr != nullptr);
    ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().realloc(ptr, new_size));
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(new_size <= block_size);
    for (size_t i = 0; i < std::min(old_size, new_size); i++) {
      EXPECT_TRUE(ptr[i] == '\0');
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };

  test(1, 15);
  test(15, 123);
  test(123, getpagesize());
  test(15, 1);
  test(123, 15);
  test(getpagesize(), 123);
}

TEST(realloc_test, realloc_to_realloc_test) {
  auto test = [](size_t old_size, size_t new_size) {
    char *ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc(10));
    EXPECT_TRUE(ptr != nullptr);
    ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().realloc(ptr, old_size));
    EXPECT_TRUE(ptr != nullptr);
    for (size_t i = 0; i < old_size; i++) {
      ptr[i] = 'A';
    }
    ptr = reinterpret_cast<char *>(GlobalAllocator::get_instance().realloc(ptr, new_size));
    size_t block_size = GlobalAllocator::get_instance().get_block_size(ptr);
    EXPECT_TRUE(new_size <= block_size);
    for (size_t i = 0; i < std::min(old_size, new_size); i++) {
      EXPECT_TRUE(ptr[i] == 'A');
    }
    GlobalAllocator::get_instance().dealloc(ptr);
  };

  test(1, 15);
  test(15, 123);
  test(123, getpagesize());
  test(15, 1);
  test(123, 15);
  test(getpagesize(), 123);
}

TEST(realloc_test, too_big_size_test) {
  void *ptr = GlobalAllocator::get_instance().alloc(100, 64);
  EXPECT_TRUE(ptr != nullptr);
  ptr = GlobalAllocator::get_instance().realloc(ptr, 0x1000000000000);
  EXPECT_TRUE(ptr == nullptr);
}

// TODO: dealloc test

TEST(MultiThreadTest, Alloc) {
  const size_t kAllocateTimes = 10000;
  const size_t kAllocSize = 0x20;

  auto thread_func = [](int **alloc_ptrs, int thread_id) {
    for (size_t i = 0; i < kAllocateTimes; i++) {
      auto ptr = malloc(kAllocSize);
      ASSERT_NE(ptr, nullptr);
      alloc_ptrs[i] = reinterpret_cast<int *>(ptr);
      *alloc_ptrs[i] = thread_id;
    }
  };

  int *alloc_ptrs[2][kAllocateTimes];
  std::thread t0(thread_func, alloc_ptrs[0], 0);
  std::thread t1(thread_func, alloc_ptrs[1], 1);

  t0.join();
  t1.join();

  for (size_t i = 0; i < kAllocateTimes; i++) {
    ASSERT_EQ(*alloc_ptrs[0][i], 0);
    ASSERT_EQ(*alloc_ptrs[1][i], 1);
  }
}