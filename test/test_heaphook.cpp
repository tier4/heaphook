
#include <cstdlib>
#include <malloc.h> // memalign, pvalloc
#include <dlfcn.h>

#include <cstdint>
#include <iostream>
#include <gtest/gtest.h>

#include "heaphook/utils.hpp"

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

TEST(malloc_test, valid_size_test) {
  auto test = [](size_t size) {
    void *ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);
    EXPECT_LE(size, malloc_usable_size(ptr));
    memset(ptr, 'A', size);
    free(ptr);
  };

  test(0); // allocates the minimum-sized chunk.
  test(1);
  test(0x20);
  test(getpagesize());
}

TEST(malloc_test, invalid_size_test) {
  errno = 0;
  void *ptr = malloc(0x100000000000000ull);
  EXPECT_EQ(nullptr, ptr);
  EXPECT_EQ(errno, ENOMEM);
}

TEST(malloc_usable_size_test, write_test) {
  auto test = [](size_t size) {
    void *ptr = malloc(size);
    ASSERT_NE(nullptr, ptr);
    size_t usable_size = malloc_usable_size(ptr);
    ASSERT_LE(size, usable_size);
    memset(ptr, 'A', usable_size);
    free(ptr);
  };

  test(0);
  test(1);
  test(0x33);
  test(0x100);
}

TEST(malloc_usable_size_test, nullptr_test) {
  ASSERT_EQ(0u, malloc_usable_size(nullptr));
}

TEST(calloc_test, valid_args_test) {
  auto test = [](size_t num, size_t size) {
    char *ptr = reinterpret_cast<char *>(calloc(num, size));
    EXPECT_NE(nullptr, ptr);
    EXPECT_LE(num * size, malloc_usable_size(ptr));
    for (size_t idx = 0; idx < num * size; idx++) {
      EXPECT_EQ(ptr[idx], '\0');
    }
    memset(ptr, 'A', num * size);
    free(ptr);
  };
  
  test(1, 1);
  test(1, 123);
  test(1, getpagesize());

  test(121, 1);
  test(121, 123);
  test(121, getpagesize());
  
  test(0, 0);
  test(0x10, 0);
  test(0, 0x10);
  test(0x10, 0x10);
}

TEST(calloc_test, invalid_args_test) {
  auto test = [](size_t num, size_t size) {
    errno = 0;
    EXPECT_EQ(calloc(num, size), nullptr);
    EXPECT_EQ(errno, ENOMEM);
  };

  test(0xdeadbeef, 0xcafebabe);
  test(0x100000000000000, 0x100000000000000); // overflow
}

// TEST(heaphook, realloc) {
//   auto test = [](size_t old_size, size_t new_size, bool is_nullptr, int expected_errno) {
//     if (is_nullptr) {
//       errno = 0;
//       EXPECT_EQ(glibc_realloc(malloc(old_size), new_size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//       errno = 0;
//       EXPECT_EQ(realloc(malloc(old_size), new_size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//     } else {
//       EXPECT_NE(glibc_realloc(malloc(old_size), new_size), nullptr);
//       EXPECT_NE(realloc(malloc(old_size), new_size), nullptr);
//     }
//   };
  
//   test(0, 0, true, 0);
//   test(0x20, 0, true, 0); // invoke free
//   test(0, 0x20, false, 0);
//   test(0x20, 0x20, false, 0);
//   test(0x20, 0x10000000000, true, ENOMEM);
// }

// TEST(heaphook, realloc2) {
//   // the case where nullptr is passed
//   auto test = [](size_t new_size, bool is_nullptr, int expected_errno) {
//     if (is_nullptr) {
//       errno = 0;
//       EXPECT_EQ(glibc_realloc(nullptr, new_size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//       errno = 0;
//       EXPECT_EQ(realloc(nullptr, new_size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//     } else {
//       EXPECT_NE(glibc_realloc(nullptr, new_size), nullptr);
//       EXPECT_NE(realloc(nullptr, new_size), nullptr);
//     }
//   };

//   // if nullptr is passed then realloc is equivalent to malloc(new_size)
//   test(0, false, 0);
//   test(0x20, false, 0);
//   test(0xdeadbeefcafeba, true, ENOMEM);
// }

TEST(posix_memalign_test, valid_args_test) {
  auto test = [](size_t alignment, size_t size) {
    void *ptr = nullptr;
    int retval = posix_memalign(&ptr, alignment, size);
    size_t addr = reinterpret_cast<size_t>(ptr);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(addr % alignment, 0u);
    EXPECT_EQ(retval, 0);
    EXPECT_LE(size, malloc_usable_size(ptr));
    memset(ptr, 'A', size);
    free(ptr);
  };

  // alignment is valid
  test(sizeof(void *), 0);
  test(sizeof(void *), 1);
  test(sizeof(void *), 123);
  test(sizeof(void *), getpagesize());
  test(getpagesize(), 0);
  test(getpagesize(), 1);
  test(getpagesize(), 123);
  test(getpagesize(), getpagesize());
}

TEST(posix_memalign_test, invalid_size_test) {
  auto test = [](size_t alignment, size_t size) {
    void *ptr = nullptr;
    int retval = posix_memalign(&ptr, alignment, size);
    EXPECT_EQ(ptr, nullptr);
    EXPECT_EQ(retval, ENOMEM);
  };

  test(0x1000000000000, 0x1000000000000);
}

TEST(posix_memalign_test, invalid_alignment_test) {
  auto test = [](size_t alignment, size_t size) {
    void *ptr = nullptr;
    int retval = posix_memalign(&ptr, alignment, size);
    EXPECT_EQ(ptr, nullptr);
    EXPECT_EQ(retval, EINVAL);
  };

  test(0, 100);
  test(1, 100);
  test(2, 100);
  test(7, 100);
  test(24, 100);
}

TEST(memalign_test, valid_args_test) {
  auto test = [](size_t alignment, size_t size) {
    void *ptr = memalign(alignment, size);
    size_t addr = reinterpret_cast<size_t>(ptr);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(addr % alignment, 0u);
    EXPECT_LE(size, malloc_usable_size(ptr));
    memset(ptr, 'A', size);
    free(ptr);
  };

  test(sizeof(void *), 0);
  test(sizeof(void *), 1);
  test(sizeof(void *), 123);
  test(sizeof(void *), getpagesize());
  test(getpagesize(), 0);
  test(getpagesize(), 1);
  test(getpagesize(), 123);
  test(getpagesize(), getpagesize());
}

TEST(memalign_test, invalid_size_test) {
  auto test = [](size_t alignment, size_t size) {
    errno = 0;
    void *ptr = memalign(alignment, size);
    EXPECT_EQ(ptr, nullptr);
    EXPECT_EQ(errno, ENOMEM);
  };

  test(sizeof(void *), 0x1000000000000);
  test(0x1000000000000, 0x1000000000000);
}

TEST(memalign_test, invalid_alignment_test) {
  auto test = [](size_t alignment, size_t size) {
    void *ptr = memalign(alignment, size);
    size_t addr = reinterpret_cast<size_t>(ptr);
    EXPECT_NE(ptr, nullptr);
    EXPECT_LE(size, malloc_usable_size(ptr));
    alignment = next_power_of_2(alignment);
    EXPECT_EQ(addr % alignment, 0u);
    memset(ptr, 'A', size);
    free(ptr);
  };

  test(0, 100);
  test(1, 100);
  test(2, 100);
  test(7, 100);
  test(24, 100);
  test(getpagesize() - 1, 100);
  test(getpagesize() + 1, 100);

  // alignment > 0x80000000'00000000
  errno = 0;
  void *ptr = memalign(0x8000000000000001, 100);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(errno, EINVAL);
}

TEST(aligned_alloc_test, valid_args_test) {
  auto test = [](size_t alignment, size_t size) {
    void *ptr = aligned_alloc(alignment, size);
    size_t addr = reinterpret_cast<size_t>(ptr);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(addr % alignment, 0u);
    EXPECT_LE(size, malloc_usable_size(ptr));
    memset(ptr, 'A', size);
    free(ptr);
  };

  test(sizeof(void *), 0);
  test(sizeof(void *), 1);
  test(sizeof(void *), 123);
  test(sizeof(void *), getpagesize());
  test(getpagesize(), 0);
  test(getpagesize(), 1);
  test(getpagesize(), 123);
  test(getpagesize(), getpagesize());
}

TEST(aligned_alloc_test, invalid_size_test) {
  auto test = [](size_t alignment, size_t size) {
    errno = 0;
    void *ptr = aligned_alloc(alignment, size);
    EXPECT_EQ(ptr, nullptr);
    EXPECT_EQ(errno, ENOMEM);
  };

  test(sizeof(void *), 0x1000000000000);
  test(0x1000000000000, 0x1000000000000);
}

// Until GLIBC 2.35, it had the same behavior as memalign.
// TEST(aligned_alloc_test, invalid_alignment_test) {
//   auto test = [](size_t alignment, size_t size) {
//     errno = 0;
//     void *ptr = aligned_alloc(alignment, size);
//     EXPECT_EQ(ptr, nullptr);
//     EXPECT_EQ(errno, EINVAL);
//   };

//   test(0, 100);
//   test(1, 100);
//   test(2, 100);
//   test(7, 100);
//   test(24, 100);
//   test(getpagesize() - 1, 100);
//   test(getpagesize() + 1, 100);
//   test(0x8000000000000001, 100);
// }

// TEST(heaphook, valloc) {
//   auto test = [](size_t size, bool is_nullptr, int expected_errno) {
//     if (is_nullptr) {
//       errno = 0;
//       EXPECT_EQ(glibc_valloc(size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//       errno = 0;
//       EXPECT_EQ(valloc(size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//     } else {
//       EXPECT_NE(glibc_valloc(size), nullptr);
//       EXPECT_NE(valloc(size), nullptr);
//     }
//   };

//   test(0, false, 0);
//   test(120, false, 0);
//   test(0x1000, false, 0);
//   test(0x1000'000'000'000, true, ENOMEM);
// }

// TEST(heaphook, PvallocTest) {
//   auto test = [](size_t size, bool is_nullptr, int expected_errno) {
//     if (is_nullptr) {
//       errno = 0;
//       EXPECT_EQ(glibc_pvalloc(size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//       errno = 0;
//       EXPECT_EQ(pvalloc(size), nullptr);
//       EXPECT_EQ(errno, expected_errno);
//     } else {
//       EXPECT_NE(glibc_pvalloc(size), nullptr);
//       EXPECT_NE(pvalloc(size), nullptr);
//     }
//   };

//   test(0, false, 0);
//   test(120, false, 0);
//   test(0x1000, false, 0);
//   test(0x1000'000'000'000, true, ENOMEM);
// }

// TEST(heaphook, MallocUsableSizeTest) {
//   auto test = [](size_t size) {
//     EXPECT_EQ(glibc_malloc_usable_size(glibc_malloc(size)), malloc_usable_size(malloc(size)));
//   };

//   test(0);
//   test(1);
//   test(0x20);
//   test(0x1000);
//   test(0xbeef);
// }