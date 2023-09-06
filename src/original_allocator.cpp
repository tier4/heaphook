#include <dlfcn.h>
#include <cstdlib>

#include "heaphook/heaphook.hpp"
#include "heaphook/hook_types.hpp"

void write_error_string(const char *s) {
  write(STDERR_FILENO, s, strlen(s));
}

void perror(const char *s) {
  write_error_string(s);
  exit(-1);
}

using namespace heaphook;

// this allocator transfers directly to GLIBC functions.
class OriginalAllocator : public GlobalAllocator {
  void* do_alloc(size_t bytes, size_t align) override {
    if (align == 1) {
      static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
      return original_malloc(bytes);
    } else {
      static memalign_type original_memalign =
        reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));
      return original_memalign(align, bytes);
    }
  }

  void do_dealloc(void* ptr) override {
    static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
    original_free(ptr);
  }

  size_t do_get_block_size(void * ptr) override {
    static malloc_usable_size_type original_malloc_usable_size = \
      reinterpret_cast<malloc_usable_size_type>(dlsym(RTLD_NEXT, "malloc_usable_size"));
    return original_malloc_usable_size(ptr);
  }

  void *do_alloc_zeroed(size_t bytes) override {
    static calloc_type original_calloc = reinterpret_cast<calloc_type>(dlsym(RTLD_NEXT, "calloc"));
    return original_calloc(bytes, 1);
  }

  void *do_realloc(void *ptr, size_t new_size) {
    static realloc_type original_realloc = reinterpret_cast<realloc_type>(dlsym(RTLD_NEXT, "realloc"));
    return original_realloc(ptr, new_size);
  }
};

GlobalAllocator &GlobalAllocator::get_instance() {
  static OriginalAllocator original;
  return original;
}
