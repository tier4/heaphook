#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include <stdio.h>
// #include <iostream>

using malloc_type = void*(*)(size_t);

static __thread bool no_hook = false;

extern "C" {

void *malloc(size_t size) {
  static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));

  if (no_hook) {
    return original_malloc(size);
  }

  no_hook = true;

  void *caller = __builtin_return_address(0);
  void *ret = original_malloc(size);
  //std::cout << caller << ": malloc(" << size << ") -> " << ret << std::endl;
  printf("%p: malloc(%lu) -> %p\n", caller, size, ret);

  no_hook = false;
  return ret;
}

} // extern "C"

