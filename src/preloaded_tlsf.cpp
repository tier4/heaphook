#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <malloc.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

#include "tlsf/tlsf.h"

using malloc_type = void*(*)(size_t);
using free_type = void(*)(void*);
using calloc_type = void*(*)(size_t, size_t);
using realloc_type = void*(*)(void*, size_t);

// Aligned allocation
using posix_memalign_type = int(*)(void **, size_t, size_t);
using memalign_type = void*(*)(size_t, size_t);
using aligned_alloc_type = void*(*)(size_t, size_t);
using valloc_type = void*(*)(size_t);
using pvalloc_type = void*(*)(size_t);

using malloc_usable_size_type = size_t(*)(void*);

/*
 * We don't hook reallocarray() function because realloc() is called from reallocarray().
*/

enum class HookType {
  Malloc,
  Free,
  Calloc,
  Realloc,
  PosixMemalign,
  Memalign,
  AlignedAlloc,
  Valloc,
  Pvalloc,
  MallocUsableSize,
};

std::string type_names[10] = {"malloc", "free", "calloc", "realloc",
  "posix_memalign", "memalign", "aligned_alloc", "valloc", "pvalloc",
  "malloc_usable_size"};

static char *mempool_ptr;
static const size_t MEMPOOL_SIZE = 1000 * 1000 * 1000;
static std::unordered_map<void*, void*> *aligned2orig;

struct LogEntry {
  HookType type;
  void* addr;
  size_t size;
  void* new_addr; // used for realloc (otherwise NULL)
};

static const size_t BUFFER_SIZE = 100000;
static const size_t LOG_BATCH_SIZE = 100;

static LogEntry log[BUFFER_SIZE];
static LogEntry log_buffer[LOG_BATCH_SIZE];

// [avail_start, avail_end)
static size_t avail_start = 0; // guarded by mtx
static size_t avail_end = 0; // guarded by mtx
#define avail_num ((avail_end - avail_start + BUFFER_SIZE) % BUFFER_SIZE)

static bool library_unloaded = false; // guarded by mtx

static pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t init_cond = PTHREAD_COND_INITIALIZER; // mempool_initialized == true
static bool mempool_initialized = false;
static bool mempool_init_started = false; // guarded by init_mtx

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

static void initialize_mempool() {
  mempool_ptr = (char *) mmap(NULL, MEMPOOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset(mempool_ptr, 0, MEMPOOL_SIZE);
  init_memory_pool(MEMPOOL_SIZE, mempool_ptr); // tlsf library function

  // aligned2orig.reserve(10000000);
}

// Do not use printf
void check_mempool_initialized() {
  if (mempool_initialized) return;

  pthread_mutex_lock(&init_mtx);

  if (!mempool_init_started) {
    mempool_init_started = true;
    pthread_mutex_unlock(&init_mtx);

    initialize_mempool();

    aligned2orig = new std::unordered_map<void*, void*>();

    mempool_initialized = true;
    pthread_cond_signal(&init_cond);
  } else {
    while (!mempool_initialized) {
      pthread_cond_wait(&init_cond, &init_mtx);
    }

    pthread_mutex_unlock(&init_mtx);
  }
}

static void* tlsf_aligned_malloc(size_t alignment, size_t size) {
  void *addr = tlsf_malloc(alignment + size);
  void* aligned = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(addr) + alignment - reinterpret_cast<uint64_t>(addr) % alignment);
  (*aligned2orig)[aligned] = addr;

  //printf("In tlsf_aligned_malloc: orig=%p -> aligned=%p\n", addr, aligned);

  return aligned;
}

extern "C" {

void *malloc(size_t size) {
  static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
  static __thread bool malloc_no_hook = false;

  if (malloc_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_malloc(size);
    else return original_malloc(size);
  }

  malloc_no_hook = true;

  check_mempool_initialized();

  //void *caller = __builtin_return_address(0);

  // void *ret = original_malloc(size);
  void *ret = tlsf_malloc(size);

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
    if (mempool_initialized) tlsf_free(ptr);
    else original_free(ptr);
    return;
  }

  free_no_hook = true;

  check_mempool_initialized();

  //void *caller = __builtin_return_address(0);
  auto it = aligned2orig->find(ptr);
  if (it != aligned2orig->end()) {
    //printf("In free: aligned=%p -> orig=%p\n", it->first, it->second);
    ptr = it->second;
    aligned2orig->erase(it);
  }

  tlsf_free(ptr);
  //printf("%p: free(%p)\n", caller, ptr);

  locked_logging(HookType::Free, ptr, 0, NULL);

  free_no_hook = false;
}

void* calloc(size_t num, size_t size) {
  static calloc_type original_calloc = reinterpret_cast<calloc_type>(dlsym(RTLD_NEXT, "calloc"));
  static __thread bool calloc_no_hook = false;

  if (calloc_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_calloc(num, size);
    else return original_calloc(num, size);
  }

  calloc_no_hook = true;

  check_mempool_initialized();

  void *ret = tlsf_calloc(num, size);

  locked_logging(HookType::Calloc, ret, num * size, NULL);

  calloc_no_hook = false;

  return ret;
}

void* realloc(void *ptr, size_t new_size) {
  static realloc_type original_realloc = reinterpret_cast<realloc_type>(dlsym(RTLD_NEXT, "realloc"));
  static __thread bool realloc_no_hook = false;

  if (realloc_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_realloc(ptr, new_size);
    else return original_realloc(ptr, new_size);
  }

  realloc_no_hook = true;

  check_mempool_initialized();

  auto it = aligned2orig->find(ptr);
  if (it != aligned2orig->end()) {
    //printf("In realloc: aligned=%p -> orig=%p\n", it->first, it->second);
    ptr = it->second;
    aligned2orig->erase(ptr);
  }

  void *ret = tlsf_realloc(ptr, new_size);

  locked_logging(HookType::Realloc, ptr, new_size, ret);

  realloc_no_hook = false;
  return ret;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  static posix_memalign_type original_posix_memalign = reinterpret_cast<posix_memalign_type>(dlsym(RTLD_NEXT, "posix_memalign"));
  static __thread bool posix_memalign_no_hook = false;

  if (posix_memalign_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) {
      *memptr = tlsf_aligned_malloc(alignment, size);
      return 0;
    } else {
      return original_posix_memalign(memptr, alignment, size);
    }
  }

  posix_memalign_no_hook = true;
  check_mempool_initialized();

  *memptr = tlsf_aligned_malloc(alignment, size);

  locked_logging(HookType::PosixMemalign, *memptr, size, NULL);
  posix_memalign_no_hook = false;
  return 0;
}

void* memalign(size_t alignment, size_t size) {
  static memalign_type original_memalign = reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));
  static __thread bool memalign_no_hook = false;

  if (memalign_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_aligned_malloc(alignment, size);
    else return original_memalign(alignment, size);
  }

  memalign_no_hook = true;
  check_mempool_initialized();

  void *ret = tlsf_aligned_malloc(alignment, size);

  locked_logging(HookType::Memalign, ret, size, NULL);

  memalign_no_hook = false;
  return ret;
}

void* aligned_alloc(size_t alignment, size_t size) {
  static aligned_alloc_type original_aligned_alloc = reinterpret_cast<aligned_alloc_type>(dlsym(RTLD_NEXT, "aligned_alloc"));
  static __thread bool aligned_alloc_no_hook = false;

  if (aligned_alloc_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_aligned_malloc(alignment, size);
    else return original_aligned_alloc(alignment, size);
  }

  aligned_alloc_no_hook = true;
  check_mempool_initialized();

  void *ret = tlsf_aligned_malloc(alignment, size);

  locked_logging(HookType::AlignedAlloc, ret, size, NULL);
  aligned_alloc_no_hook = false;
  return ret;
}

void* valloc(size_t size) {
  static valloc_type original_valloc = reinterpret_cast<valloc_type>(dlsym(RTLD_NEXT, "valloc"));
  static __thread bool valloc_no_hook = false;
  static size_t page_size = sysconf(_SC_PAGESIZE);

  if (valloc_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_aligned_malloc(page_size, size);
    else return original_valloc(size);
  }

  valloc_no_hook = true;
  check_mempool_initialized();

  void *ret = tlsf_aligned_malloc(page_size, size);

  locked_logging(HookType::Valloc, ret, size, NULL);
  valloc_no_hook = false;

  return ret;
}

void* pvalloc(size_t size) {
  static pvalloc_type original_pvalloc = reinterpret_cast<pvalloc_type>(dlsym(RTLD_NEXT, "pvalloc"));
  static __thread bool pvalloc_no_hook = false;
  static size_t page_size = sysconf(_SC_PAGESIZE);
  size_t rounded_up = size + (page_size - size % page_size) % page_size;

  if (pvalloc_no_hook || pthread_self() == logging_thread) {
    if (mempool_initialized) return tlsf_aligned_malloc(page_size, rounded_up);
    return original_pvalloc(size);
  }

  pvalloc_no_hook = true;
  check_mempool_initialized();

  void *ret = tlsf_aligned_malloc(page_size, rounded_up);

  // pvalloc() rounds the size of the allocation up to the next multiple of the system page size.
  locked_logging(HookType::Pvalloc, ret, rounded_up, NULL);

  return ret;
}

size_t malloc_usable_size(void *ptr) {
  static malloc_usable_size_type original_malloc_usable_size = \
    reinterpret_cast<malloc_usable_size_type>(dlsym(RTLD_NEXT, "malloc_usable_size"));

  check_mempool_initialized();

  printf("hoge: malloc_usable_size called\n");

  size_t ret = original_malloc_usable_size(ptr);
  locked_logging(HookType::MallocUsableSize, ptr, ret, NULL);
  return ret;
}

using mallinfo_type = struct mallinfo(*)(void);
struct mallinfo mallinfo() {
  static mallinfo_type orig = reinterpret_cast<mallinfo_type>(dlsym(RTLD_NEXT, "mallinfo"));
  printf("hoge: mallinfo called\n");
  return orig();
}

using mallinfo2_type = struct mallinfo2(*)(void);
struct mallinfo2 mallinfo2() {
  static mallinfo2_type orig = reinterpret_cast<mallinfo2_type>(dlsym(RTLD_NEXT, "mallinfo2"));
  printf("hoge: mallinfo2 called\n");
  return orig();
}

using mallopt_type = int(*)(int, int);
int mallopt(int param, int value) {
  static mallopt_type orig = reinterpret_cast<mallopt_type>(dlsym(RTLD_NEXT, "mallopt"));
  printf("hoge: mallopt called\n");
  return orig(param, value);
}

using malloc_trim_type = int(*)(size_t);
int malloc_trim(size_t pad) {
  static malloc_trim_type orig = reinterpret_cast<malloc_trim_type>(dlsym(RTLD_NEXT, "malloc_trim"));
  printf("hoge: malloc_trim called\n");
  return orig(pad);
}

using malloc_stats_type = void(*)(void);
void malloc_stats(void) {
  static malloc_stats_type orig = reinterpret_cast<malloc_stats_type>(dlsym(RTLD_NEXT, "malloc_stats"));
  printf("hoge: malloc_stats\n");
  orig();
  return;
}

using malloc_info_type = int(*)(int, FILE*);
int malloc_info(int options, FILE *stream) {
  static malloc_info_type orig = reinterpret_cast<malloc_info_type>(dlsym(RTLD_NEXT, "malloc_info"));
  printf("malloc_info called\n");
  return orig(options, stream);
}

} // extern "C"
