#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include <stdio.h>
// #include <iostream>

using malloc_type = void*(*)(size_t);
using free_type = void(*)(void*);

static const int LOG_SIZE = 100000;
static int sizes_log[LOG_SIZE];
static void* addrs_log[LOG_SIZE];
static int log_num = 0;

static void __attribute__((constructor)) init() {
  printf("preloaded.so loaded!\n");

  // first touch
  for (int i = log_num; i < LOG_SIZE; i++) {
    sizes_log[i] = 0;
    addrs_log[i] = 0;
  }
}

static void __attribute__((destructor)) fini() {
  printf("preloaded.so unloaded!\n");

  for (int i = 0; i < log_num; i++) {
    printf("%d: %p, %d\n", i, addrs_log[i], sizes_log[i]);
  }
}

extern "C" {

void *malloc(size_t size) {
  static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
  static __thread bool malloc_no_hook = false;

  if (malloc_no_hook) {
    return original_malloc(size);
  }

  malloc_no_hook = true;

  void *caller = __builtin_return_address(0);
  void *ret = original_malloc(size);
  //std::cout << caller << ": malloc(" << size << ") -> " << ret << std::endl;
  printf("%p: malloc(%lu) -> %p\n", caller, size, ret);
  sizes_log[log_num] = size;
  addrs_log[log_num++] = ret;

  malloc_no_hook = false;
  return ret;
}

void free(void* ptr) {
  static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
  static __thread bool free_no_hook = false;

  if (free_no_hook) {
    return original_free(ptr);
  }

  free_no_hook = true;

  void *caller = __builtin_return_address(0);
  original_free(ptr);
  printf("%p: free(%p)\n", caller, ptr);
  sizes_log[log_num] = -1;
  addrs_log[log_num++] = ptr;

  free_no_hook = false;
}

} // extern "C"

