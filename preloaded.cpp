#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include <stdio.h>
// #include <iostream>

using malloc_type = void*(*)(size_t);
using free_type = void(*)(void*);

static __thread bool malloc_no_hook = false;
static __thread bool free_no_hook = false;

extern "C" {

void *malloc(size_t size) {
  static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));

  if (malloc_no_hook) {
    return original_malloc(size);
  }

  malloc_no_hook = true;

  void *caller = __builtin_return_address(0);
  void *ret = original_malloc(size);
  //std::cout << caller << ": malloc(" << size << ") -> " << ret << std::endl;
  printf("%p: malloc(%lu) -> %p\n", caller, size, ret);

  malloc_no_hook = false;
  return ret;
}

void free(void* ptr) {
  static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));

  if (free_no_hook) {
    return original_free(ptr);
  }

  free_no_hook = true;

  void *caller = __builtin_return_address(0);
  free(ptr);
  printf("%p: free(%p)\n", caller, ptr);

  free_no_hook = false;
}

} // extern "C"

