#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <string>

using malloc_type = void*(*)(size_t);
using free_type = void(*)(void*);
using calloc_type = void*(*)(size_t, size_t);
using realloc_type = void*(*)(void*, size_t);

enum class HookType {
  Malloc,
  Free,
  Calloc,
  Realloc,
};

std::string type_names[4] = {"malloc", "free", "calloc", "realloc"};

struct LogEntry {
  HookType type;
  void* addr;
  size_t size;
  void* new_addr; // used for realloc (otherwise NULL)
};

static const size_t BUFFER_SIZE = 10000000;
static const size_t LOG_BATCH_SIZE = 100;

static LogEntry log[BUFFER_SIZE];
static LogEntry log_buffer[LOG_BATCH_SIZE];

// [avail_start, avail_end)
static size_t avail_start = 0; // guarded by mtx
static size_t avail_end = 0; // guarded by mtx
#define avail_num ((avail_end - avail_start + BUFFER_SIZE) % BUFFER_SIZE)

static bool library_unloaded = false; // guarded by mtx

static pthread_t logging_thread;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full_cond = PTHREAD_COND_INITIALIZER; // avail_num < (BUFFER_SIZE - 1)
static pthread_cond_t not_empty_cond = PTHREAD_COND_INITIALIZER; // avail_num > 0

static void* logging_thread_fn(void *arg) {
  (void) arg;

  while (true) {
    pthread_mutex_lock(&mtx);
    while (avail_num == 0) {
      pthread_cond_wait(&not_empty_cond, &mtx);
    }

    size_t sz = std::min(LOG_BATCH_SIZE, avail_num);
    for (size_t i = 0; i < sz; i++) {
      log_buffer[i] = log[avail_start + i];
    }

    bool was_full = (avail_num == BUFFER_SIZE - 1);
    avail_start = (avail_start + sz) % BUFFER_SIZE;

    pthread_mutex_unlock(&mtx);

    if (was_full) {
      pthread_cond_broadcast(&not_full_cond);
    }

    std::ofstream ofs("heaplog." + std::to_string(getpid()) + ".log", std::ios::app);
    for (size_t i = 0; i < sz; i++) {
      ofs << type_names[static_cast<int>(log_buffer[i].type)] << " " << log_buffer[i].addr
        << " " << log_buffer[i].size << " " << log_buffer[i].new_addr << std::endl;
    }
    ofs.close();

    pthread_mutex_lock(&mtx);

    if (library_unloaded && avail_num == 0) {
      pthread_mutex_unlock(&mtx);
      pthread_exit(NULL);
    }

    pthread_mutex_unlock(&mtx);
  }
}

static void locked_logging(HookType hook_type, void *addr, size_t size, void *new_addr) {
  pthread_mutex_lock(&mtx);

  while (avail_num == BUFFER_SIZE - 1) {
    fprintf(stderr, "Warning: Logging buffer is full. free() is temporarily blocked.\n");
    pthread_cond_wait(&not_full_cond, &mtx);
  }

  if (!library_unloaded) {
    log[avail_end] = {hook_type, addr, size, new_addr};
    avail_end = (avail_end + 1) % BUFFER_SIZE;
  }

  bool was_empty = (avail_num == 1);
  pthread_mutex_unlock(&mtx);

  if (was_empty) {
    pthread_cond_signal(&not_empty_cond);
  }
}

static void __attribute__((destructor)) fini() {
  pthread_mutex_lock(&mtx);
  library_unloaded = true;
  pthread_mutex_unlock(&mtx);

  pthread_join(logging_thread, NULL);
}

static void __attribute__((constructor)) init() {
  pthread_mutex_lock(&mtx);

  // first touch
  for (size_t i = avail_end; i < BUFFER_SIZE; i++) {
    log[i] = {};
  }

  for (size_t i = 0; i < LOG_BATCH_SIZE; i++) {
    log_buffer[i] = {};
  }

  pthread_mutex_unlock(&mtx);

  pthread_create(&logging_thread, NULL, logging_thread_fn, 0);
}

extern "C" {

void *malloc(size_t size) {
  static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
  static __thread bool malloc_no_hook = false;

  if (malloc_no_hook || pthread_self() == logging_thread) {
    return original_malloc(size);
  }

  malloc_no_hook = true;

  void *caller = __builtin_return_address(0);
  void *ret = original_malloc(size);
  //std::cout << caller << ": malloc(" << size << ") -> " << ret << std::endl;
  //printf("%p: malloc(%lu) -> %p\n", caller, size, ret);

  locked_logging(HookType::Malloc, ret, size, NULL);

  malloc_no_hook = false;
  return ret;
}

void free(void* ptr) {
  static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
  static __thread bool free_no_hook = false;

  if (free_no_hook || pthread_self() == logging_thread) {
    return original_free(ptr);
  }

  free_no_hook = true;

  void *caller = __builtin_return_address(0);
  original_free(ptr);
  //printf("%p: free(%p)\n", caller, ptr);

  locked_logging(HookType::Free, ptr, 0, NULL);

  free_no_hook = false;
}

void* calloc(size_t num, size_t size) {
  static calloc_type original_calloc = reinterpret_cast<calloc_type>(dlsym(RTLD_NEXT, "calloc"));
  static __thread bool calloc_no_hook = false;

  if (calloc_no_hook || pthread_self() == logging_thread) {
    return original_calloc(num, size);
  }

  calloc_no_hook = true;

  void *ret = original_calloc(num, size);

  locked_logging(HookType::Calloc, ret, num * size, NULL);

  calloc_no_hook = false;

  return ret;
}

void* realloc(void *ptr, size_t new_size) {
  static realloc_type original_realloc = reinterpret_cast<realloc_type>(dlsym(RTLD_NEXT, "realloc"));
  static __thread bool realloc_no_hook = false;

  if (realloc_no_hook || pthread_self() == logging_thread) {
    return original_realloc(ptr, new_size);
  }

  realloc_no_hook = true;
  void *ret = original_realloc(ptr, new_size);

  locked_logging(HookType::Realloc, ptr, new_size, ret);

  realloc_no_hook = false;
  return ret;
}

} // extern "C"
