
#include <cstdlib>
#include <malloc.h> // memalign, pvalloc
#include <dlfcn.h>

#include <iostream>
#include <gtest/gtest.h>

using malloc_type = void * (*)(size_t);
using free_type = void (*)(void *);
using calloc_type = void * (*)(size_t, size_t);
using realloc_type = void * (*)(void *, size_t);

using posix_memalign_type = int (*)(void **, size_t, size_t);
using memalign_type = void * (*)(size_t, size_t);
using aligned_alloc_type = void * (*)(size_t, size_t);
using valloc_type = void * (*)(size_t);
using pvalloc_type = void * (*)(size_t);

using malloc_usable_size_type = size_t (*)(void *);

// glibc implementation
void *glibc_malloc(size_t size) {
  static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
  return original_malloc(size);
}

void glibc_free(void *ptr) {
  static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
  original_free(ptr);
}

void *glibc_calloc(size_t num, size_t size) {
  static calloc_type original_calloc = reinterpret_cast<calloc_type>(dlsym(RTLD_NEXT, "calloc"));
  return original_calloc(num, size);
}

void *glibc_realloc(void * ptr, size_t new_size) {
  static realloc_type original_realloc = reinterpret_cast<realloc_type>(dlsym(RTLD_NEXT, "realloc"));
  return original_realloc(ptr, new_size);
}

int glibc_posix_memalign(void ** memptr, size_t alignment, size_t size) {
  static posix_memalign_type original_posix_memalign = reinterpret_cast<posix_memalign_type>(dlsym(RTLD_NEXT, "posix_memalign"));
  return original_posix_memalign(memptr, alignment, size);
}

void *glibc_memalign(size_t alignment, size_t size) {
  static memalign_type original_memalign = reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));
  return original_memalign(alignment, size);
}

void *glibc_aligned_alloc(size_t alignment, size_t size) {
  static aligned_alloc_type original_aligned_alloc = reinterpret_cast<aligned_alloc_type>(dlsym(RTLD_NEXT, "aligned_alloc"));
  return original_aligned_alloc(alignment, size);
}

void *glibc_valloc(size_t size) {
  static valloc_type original_valloc = reinterpret_cast<valloc_type>(dlsym(RTLD_NEXT, "valloc"));
  return original_valloc(size);
}

void *glibc_pvalloc(size_t size) {
  static pvalloc_type original_pvalloc = reinterpret_cast<pvalloc_type>(dlsym(RTLD_NEXT, "pvalloc"));
  return original_pvalloc(size);
}

size_t glibc_malloc_usable_size(void *ptr) {
  static malloc_usable_size_type original_malloc_usable_size = reinterpret_cast<malloc_usable_size_type>(dlsym(RTLD_NEXT, "malloc_usable_size"));
  return original_malloc_usable_size(ptr);
}

// Is the timing of returning nullptr the same ?
TEST(HeaphookTest, MallocTest) {
  // test if the timing of returning nullptr is the same.
  auto test = [](size_t size, bool is_nullptr, int expected_errno) {
    if (is_nullptr) {
      errno = 0;
      EXPECT_EQ(glibc_malloc(size), nullptr);
      EXPECT_EQ(errno, expected_errno);
      errno = 0;
      EXPECT_EQ(malloc(size), nullptr);
      EXPECT_EQ(errno, expected_errno);
    } else {
      EXPECT_NE(glibc_malloc(size), nullptr);
      EXPECT_NE(malloc(size), nullptr);
    }
  };

  // if size == 0 then malloc allocates the minimum-sized chunk.
  test(0, false, 0);
  test(1, false, 0);
  test(0x20, false, 0);
  test(0xdeadbeefcafeba, true, ENOMEM); // exceeding the maximum size
}

TEST(HeaphookTest, CallocTest) {
  // test if the timing of returning nullptr is the same.
  auto test = [](size_t num, size_t size, bool is_nullptr, int expected_errno) {
    if (is_nullptr) {
      errno = 0;
      EXPECT_EQ(glibc_calloc(num, size), nullptr);
      EXPECT_EQ(errno, expected_errno);
      errno = 0;
      EXPECT_EQ(calloc(num, size), nullptr);
      EXPECT_EQ(errno, expected_errno);
    } else {
      EXPECT_NE(glibc_calloc(num, size), nullptr);
      EXPECT_NE(calloc(num, size), nullptr);
    }
  };
  
  // if num * size == 0 then
  // calloc allocates the minimum-sized chunk, similar to malloc.
  test(0, 0, false, 0);
  test(0x10, 0, false, 0);
  test(0, 0x10, false, 0);
  test(0x10, 0x10, false, 0);
  test(0xdeadbeef, 0xcafebabe, true, ENOMEM); // allocation failed due to size being too large
  test(0x100000000, 0x100000000, true, ENOMEM); // overflow
}

TEST(HeaphookTest, ReallocTest1) {
  auto test = [](size_t old_size, size_t new_size, bool is_nullptr, int expected_errno) {
    if (is_nullptr) {
      errno = 0;
      EXPECT_EQ(glibc_realloc(malloc(old_size), new_size), nullptr);
      EXPECT_EQ(errno, expected_errno);
      errno = 0;
      EXPECT_EQ(realloc(malloc(old_size), new_size), nullptr);
      EXPECT_EQ(errno, expected_errno);
    } else {
      EXPECT_NE(glibc_realloc(malloc(old_size), new_size), nullptr);
      EXPECT_NE(realloc(malloc(old_size), new_size), nullptr);
    }
  };
  
  test(0, 0, true, 0);
  test(0x20, 0, true, 0); // invoke free
  test(0, 0x20, false, 0);
  test(0x20, 0x20, false, 0);
  test(0x20, 0x10000000000, true, ENOMEM);
}

TEST(HeaphookTest, ReallocTest2) {
  // the case where nullptr is passed
  auto test = [](size_t new_size, bool is_nullptr, int expected_errno) {
    if (is_nullptr) {
      errno = 0;
      EXPECT_EQ(glibc_realloc(nullptr, new_size), nullptr);
      EXPECT_EQ(errno, expected_errno);
      errno = 0;
      EXPECT_EQ(realloc(nullptr, new_size), nullptr);
      EXPECT_EQ(errno, expected_errno);
    } else {
      EXPECT_NE(glibc_realloc(nullptr, new_size), nullptr);
      EXPECT_NE(realloc(nullptr, new_size), nullptr);
    }
  };

  // if nullptr is passed then realloc is equivalent to malloc(new_size)
  test(0, false, 0);
  test(0x20, false, 0);
  test(0xdeadbeefcafeba, true, ENOMEM);
}

TEST(HeaphookTest, PosixMemalignTest) {
  auto test = [](size_t size, size_t alignment, bool is_nullptr, int retval) {
    void *glibc_ptr = nullptr;
    void *heaphook_ptr = nullptr;
    int glibc_retval, heaphook_retval;
    glibc_retval = glibc_posix_memalign(&glibc_ptr, alignment, size);
    heaphook_retval = posix_memalign(&heaphook_ptr, alignment, size);
    if (is_nullptr) {
      EXPECT_EQ(glibc_ptr, nullptr);
      EXPECT_EQ(heaphook_ptr, nullptr);
    } else {
      EXPECT_NE(glibc_ptr, nullptr);
      EXPECT_NE(heaphook_ptr, nullptr);
    }
    EXPECT_EQ(glibc_retval, retval);
    EXPECT_EQ(heaphook_retval, retval);
  };

  // alignment is valid
  test(0, 8, false, 0);
  test(1, 8, false, 0);
  test(0x20, 8, false, 0);
  test(0x1'000'000'000'000, 8, true, ENOMEM); // too large size
  // alignment is invalid
  test(0x20, 0, true, EINVAL);
  test(0x20, 1, true, EINVAL);
  test(0x20, 3, true, EINVAL);
  test(0x20, 24, true, EINVAL);
  test(0x1'000'000'000'000, 1, true, EINVAL); // too large size
}

TEST(HeaphookTest, VallocTest) {
  auto test = [](size_t size, bool is_nullptr, int expected_errno) {
    if (is_nullptr) {
      errno = 0;
      EXPECT_EQ(glibc_valloc(size), nullptr);
      EXPECT_EQ(errno, expected_errno);
      errno = 0;
      EXPECT_EQ(valloc(size), nullptr);
      EXPECT_EQ(errno, expected_errno);
    } else {
      EXPECT_NE(glibc_valloc(size), nullptr);
      EXPECT_NE(valloc(size), nullptr);
    }
  };

  test(0, false, 0);
  test(120, false, 0);
  test(0x1000, false, 0);
  test(0x1000'000'000'000, true, ENOMEM);
}

TEST(HeaphookTest, PvallocTest) {
  auto test = [](size_t size, bool is_nullptr, int expected_errno) {
    if (is_nullptr) {
      errno = 0;
      EXPECT_EQ(glibc_pvalloc(size), nullptr);
      EXPECT_EQ(errno, expected_errno);
      errno = 0;
      EXPECT_EQ(pvalloc(size), nullptr);
      EXPECT_EQ(errno, expected_errno);
    } else {
      EXPECT_NE(glibc_pvalloc(size), nullptr);
      EXPECT_NE(pvalloc(size), nullptr);
    }
  };

  test(0, false, 0);
  test(120, false, 0);
  test(0x1000, false, 0);
  test(0x1000'000'000'000, true, ENOMEM);
}

TEST(HeaphookTest, MallocUsableSizeTest) {
  auto test = [](size_t size) {
    EXPECT_EQ(glibc_malloc_usable_size(glibc_malloc(size)), malloc_usable_size(malloc(size)));
  };

  test(0);
  test(1);
  test(0x20);
  test(0x1000);
  test(0xbeef);
}