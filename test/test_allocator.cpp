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

TEST(AllocTest, SizeTest) {
  const size_t kSizeMax = 2 * 4096;
  for (size_t size = 1; size < kSizeMax; size++) {
    char *p = (char *) GlobalAllocator::get_instance().alloc(size);
    EXPECT_NE(nullptr, p);
    for (size_t idx = 0; idx < size; idx++) p[idx] = 'A';
  }

  // fail
  void *p = GlobalAllocator::get_instance().alloc(0xffffffffffffffff);
  EXPECT_EQ(p, nullptr);
}

TEST(AllocTest, AlignTest) {
  void *p;
  size_t pval;
  size_t align = sizeof(void *);
  for (int i = 0; i < 12; i++, align <<= 1) {
    p = GlobalAllocator::get_instance().alloc(0x20, align);
    pval = reinterpret_cast<size_t>(p);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(pval % align, 0ull);
    GlobalAllocator::get_instance().dealloc(p);
  }

  align = 0x100;
  for (auto size: { 1,3, 7, 9, 15, 0xff, 0x102, 0x999 }) {
    p = GlobalAllocator::get_instance().alloc(size, align);
    pval = reinterpret_cast<size_t>(p);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(pval % align, 0ull);
    GlobalAllocator::get_instance().dealloc(p);
  }
}

// is alloc_zeroed properly initialized with 0 ?
TEST(AllocZeroedTest, ZeroedTest) {
  const size_t kSizeMax = 0x100;
  for (size_t size = 1; size < kSizeMax; size++) {
    char *p = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc_zeroed(size));
    EXPECT_NE(nullptr, p);
    for (size_t idx = 0; idx < size; idx++) EXPECT_EQ(0, p[idx]);
  }

  for (size_t size = 0x100; size < 0x10000; size <<= 1) {
    char *p = reinterpret_cast<char *>(GlobalAllocator::get_instance().alloc_zeroed(size));
    EXPECT_NE(nullptr, p);
    for (size_t idx = 0; idx < size; idx++) EXPECT_EQ(0, p[idx]);
  }
}

TEST(GetBlockSizeTest, SizeTest) {
  const size_t kSizeMax = 2 * 4096;
  for (size_t size = 1; size < kSizeMax; size++) {
    void *p = GlobalAllocator::get_instance().alloc(size);
    EXPECT_NE(nullptr, p);
    EXPECT_LE(size, GlobalAllocator::get_instance().get_block_size(p));
    GlobalAllocator::get_instance().dealloc(p);
  }
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