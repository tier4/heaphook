#include <dlfcn.h>
#include <cstdlib>
#include <cerrno>

#include "heaphook/heaphook.hpp"
#include "heaphook/hook_types.hpp"
#include "heaphook/utils.hpp"

using namespace heaphook;

static inline void *_int_malloc(size_t size) {
  if (size == 0) 
    size++;

  // size > 0
  void *retval = GlobalAllocator::get_instance().alloc(size);
  if (retval == nullptr)
    errno = ENOMEM;
  return retval;
}

static inline void _int_free(void *ptr) {
  if (ptr == nullptr) 
    return;

  // ptr != nullptr
  return GlobalAllocator::get_instance().dealloc(ptr);
}

static inline void *_int_calloc(size_t num, size_t size) {
  size_t bytes;
  if (__glibc_unlikely(__builtin_mul_overflow (num, size, &bytes))) {
    // write_to_stderr("\n[ heaphook ] calloc: an overflow occurred in multiplication, num = ", num, ", size = ", size, "\n");
    errno = ENOMEM;
    return nullptr;
  }
  if (bytes == 0)
    bytes++;

  // bytes > 0
  void *retval = heaphook::GlobalAllocator::get_instance().alloc_zeroed(bytes);
  if (retval == nullptr)
    errno = ENOMEM;
  return retval;
}

static inline void *_int_realloc(void *ptr, size_t new_size) {
  if (new_size == 0) {
    _int_free(ptr);
    return nullptr;
  }
  if (ptr == nullptr)
    return _int_malloc(new_size);

  // ptr != nullptr && new_size > 0
  void *retval = heaphook::GlobalAllocator::get_instance().realloc(ptr, new_size);
  if (retval == nullptr)
    errno = ENOMEM;
  return retval;
}

static inline int _int_posix_memalign(void ** memptr, size_t alignment, size_t size) {
  if (!is_valid_alignment(alignment))
    return EINVAL;

  if (size == 0)
    size++;

  // alignment = 2^k && alignment is multiple of sizeof(void *) && size > 0
  void *ptr = GlobalAllocator::get_instance().alloc(size, alignment);
  if (ptr != nullptr) {
    *memptr = ptr;
    return 0;
  }
  return ENOMEM;
}

// this function is obsolete
static inline void *_int_memalign(size_t alignment, size_t size) {
  if (!is_valid_alignment(alignment))
    alignment = next_power_of_2(alignment);
  if (alignment == 0) { // alignment > 0x80000000'00000000
    errno = EINVAL;
    return nullptr;
  }
  if (size == 0)
    size++;
  
  // alignment = 2^k && alignment is multiple of sizeof(void *) && size > 0
  void *retval = GlobalAllocator::get_instance().alloc(size, alignment);
  if (retval == nullptr)
    errno = ENOMEM;
  return retval;
}

// this implementation is compliant with ISO C17 and cannot be tested.
static inline void *_int_aligned_alloc(size_t alignment, size_t size) {
  if (!is_valid_alignment(alignment)) {
    errno = EINVAL;
    return nullptr;
  }

  if (size == 0)
    size++;
  
  // alignment = 2^k && alignment is multiple of sizeof(void *) && size > 0
  void *retval = GlobalAllocator::get_instance().alloc(size, alignment);
  if (retval == nullptr)
    errno = ENOMEM;
  return retval;
}

// this function is obsolete
static inline void *_int_valloc(size_t size) {
  static size_t page_size = sysconf(_SC_PAGESIZE);
  return _int_memalign(page_size, size);
}

// this function is obsolete
static inline void *_int_pvalloc(size_t size) {
  static size_t page_size = sysconf(_SC_PAGESIZE);
  size_t rounded_bytes;
  if (__glibc_unlikely(__builtin_add_overflow (size, page_size - 1, &rounded_bytes))) {
    errno = ENOMEM;
    return nullptr;
  }
  rounded_bytes = rounded_bytes & -(page_size - 1);
  return _int_memalign(page_size, rounded_bytes);
}

static inline size_t _int_malloc_usable_size(void * ptr) {
  if (ptr == nullptr) {
    return 0;
  }
  // ptr != nullptr
  return GlobalAllocator::get_instance().get_block_size(ptr);
}

extern "C" {

void *malloc(size_t size) {
  return _int_malloc(size);
}

void free(void * ptr) {
  _int_free(ptr);
}

void *calloc(size_t num, size_t size) {
  return _int_calloc(num, size);
}


void *realloc(void *ptr, size_t new_size) {
  return _int_realloc(ptr, new_size);
}


int posix_memalign(void ** memptr, size_t alignment, size_t size) {
  return _int_posix_memalign(memptr, alignment, size);
}

void *memalign(size_t alignment, size_t size) {
  return _int_memalign(alignment, size);
}

void *aligned_alloc(size_t alignment, size_t size) {
  return _int_aligned_alloc(alignment, size);
}

void *valloc(size_t size) {
  return _int_valloc(size);
}

void *pvalloc(size_t size) {
  return _int_pvalloc(size);
}

size_t malloc_usable_size(void * ptr) {
  return _int_malloc_usable_size(ptr);
}

} // extern "C"