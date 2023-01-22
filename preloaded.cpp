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

static const size_t BUFFER_SIZE = 100000;
static const size_t LOG_BATCH_SIZE = 100;

static int sizes_log[BUFFER_SIZE];
static void* addrs_log[BUFFER_SIZE];
static int sizes_log_buffer[LOG_BATCH_SIZE];
static void* addrs_log_buffer[LOG_BATCH_SIZE];

// [avail_start, avail_end)
static size_t avail_start = 0; // guarded by mtx
static size_t avail_end = 0; // guarded by mtx
#define avail_num ((avail_end - avail_start + BUFFER_SIZE) % BUFFER_SIZE)

bool library_unloaded = false; // guarded by mtx

static pthread_t logging_thread;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full_cond = PTHREAD_COND_INITIALIZER; // avail_num < (BUFFER_SIZE - 1)
static pthread_cond_t not_empty_cond = PTHREAD_COND_INITIALIZER; // avail_num > 0

void* logging_thread_fn(void *arg) {
  (void) arg;

  while (true) {
    printf("while loop start in logging thread (avail_num=%ld)\n", avail_num);
    pthread_mutex_lock(&mtx);
    while (avail_num == 0) {
      printf("pthread_cond_wait(not_empty_cond)\n");
      pthread_cond_wait(&not_empty_cond, &mtx);
    }

    size_t sz = std::min(LOG_BATCH_SIZE, avail_num);
    for (size_t i = 0; i < sz; i++) {
      sizes_log_buffer[i] = sizes_log[avail_start + i];
      addrs_log_buffer[i] = addrs_log[avail_start + i];
    }

    avail_start = (avail_start + sz) % BUFFER_SIZE;

    pthread_mutex_unlock(&mtx);
    pthread_cond_signal(&not_full_cond);

    std::ofstream ofs("heaplog." + std::to_string(getpid()) + ".log", std::ios::app);
    for (size_t i = 0; i < sz; i++) {
      ofs << addrs_log_buffer[i] << " " << sizes_log_buffer[i] << std::endl;
    }
    ofs.close();

    printf("while loop end in logging thread (avail_num=%ld)\n", avail_num);

    pthread_mutex_lock(&mtx);

    if (library_unloaded && avail_num == 0) {
      pthread_mutex_unlock(&mtx);
      pthread_exit(NULL);
    }

    pthread_mutex_unlock(&mtx);
  }
}

static void __attribute__((destructor)) fini() {
  printf("preloaded.so unloaded!\n");

  pthread_mutex_lock(&mtx);
  library_unloaded = true;
  pthread_mutex_unlock(&mtx);

  pthread_join(logging_thread, NULL);
}

static void __attribute__((constructor)) init() {
  printf("preloaded.so loaded!\n");

  pthread_mutex_lock(&mtx);

  // first touch
  for (size_t i = avail_end; i < BUFFER_SIZE; i++) {
    sizes_log[i] = 0;
    addrs_log[i] = 0;
  }

  for (size_t i = 0; i < LOG_BATCH_SIZE; i++) {
    sizes_log_buffer[i] = 0;
    addrs_log_buffer[i] = 0;
  }

  pthread_mutex_unlock(&mtx);

  pthread_create(&logging_thread, NULL, logging_thread_fn, 0);
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

  pthread_mutex_lock(&mtx);
  while (avail_num == BUFFER_SIZE - 1) {
    pthread_cond_wait(&not_full_cond, &mtx);
  }

  if (!library_unloaded) {
    sizes_log[avail_end] = size;
    addrs_log[avail_end] = ret;
    avail_end = (avail_end + 1) % BUFFER_SIZE;
  }

  pthread_mutex_unlock(&mtx);
  pthread_cond_signal(&not_empty_cond);

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

  pthread_mutex_lock(&mtx);
  while (avail_num == BUFFER_SIZE - 1) {
    pthread_cond_wait(&not_full_cond, &mtx);
  }

  if (!library_unloaded) {
    sizes_log[avail_end] = -1;
    addrs_log[avail_end] = ptr;
    avail_end = (avail_end + 1) % BUFFER_SIZE;
  }

  pthread_mutex_unlock(&mtx);
  pthread_cond_signal(&not_empty_cond);

  free_no_hook = false;
}

} // extern "C"

