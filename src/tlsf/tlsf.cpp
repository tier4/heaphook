#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <malloc.h>

#include <cstdint>
#include <unordered_map>
#include <string>

#include "tlsf/tlsf.h"

#include "heaphook/heaphook.hpp"
#include "heaphook/hook_types.hpp"
#include "heaphook/utils.hpp"

static char * mempool_ptr;
static size_t INITIAL_MEMPOOL_SIZE = 100 * 1000 * 1000; // default: 100MB
static size_t ADDITIONAL_MEMPOOL_SIZE = 100 * 1000 * 1000; // default: 100MB
static std::unordered_map<void *, void *> * aligned2orig;

static pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t init_cond = PTHREAD_COND_INITIALIZER; // mempool_initialized == true
static bool mempool_initialized = false;
static bool mempool_init_started = false; // guarded by init_mtx

static pthread_mutex_t tlsf_mtx = PTHREAD_MUTEX_INITIALIZER;

static void initialize_mempool()
{
  if (const char * env_p = std::getenv("INITIAL_MEMPOOL_SIZE")) {
    INITIAL_MEMPOOL_SIZE = std::stoull(std::string(env_p));
  }

  if (const char * env_p = std::getenv("ADDITIONAL_MEMPOOL_SIZE")) {
    ADDITIONAL_MEMPOOL_SIZE = std::stoull(std::string(env_p));
  }

  mempool_ptr = (char *) mmap(
    NULL, INITIAL_MEMPOOL_SIZE, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset(mempool_ptr, 0, INITIAL_MEMPOOL_SIZE);
  init_memory_pool(INITIAL_MEMPOOL_SIZE, mempool_ptr); // tlsf library function

  // aligned2orig.reserve(10000000);
}

// Do not use printf
void check_mempool_initialized()
{
  if (mempool_initialized) {return;}

  pthread_mutex_lock(&init_mtx);

  if (!mempool_init_started) {
    mempool_init_started = true;
    pthread_mutex_unlock(&init_mtx);

    initialize_mempool();
    aligned2orig = new std::unordered_map<void *, void *>();

    mempool_initialized = true;
    pthread_cond_signal(&init_cond);
  } else {
    while (!mempool_initialized) {
      pthread_cond_wait(&init_cond, &init_mtx);
    }

    pthread_mutex_unlock(&init_mtx);
  }
}

template<class F>
static void * tlsf_allocate_internal(F allocate)
{
  pthread_mutex_lock(&tlsf_mtx);

  void * ret = allocate();

  size_t multiplier = 1;
  while (ret == NULL) {
    char * addr = (char *) mmap(
      NULL, multiplier * ADDITIONAL_MEMPOOL_SIZE, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    add_new_area(addr, multiplier * ADDITIONAL_MEMPOOL_SIZE, mempool_ptr); // tlsf library function
    fprintf(
      stderr, "TLSF memory pool exhausted: %lu bytes additionally mmaped.\n",
      multiplier * ADDITIONAL_MEMPOOL_SIZE);

    ret = allocate();
    multiplier *= 2;
  }

  pthread_mutex_unlock(&tlsf_mtx);
  return ret;
}

static void * tlsf_malloc_wrapped(size_t size)
{
  return tlsf_allocate_internal([size] {return tlsf_malloc(size);});
}

static void * tlsf_realloc_wrapped(void * ptr, size_t new_size)
{
  return tlsf_allocate_internal([ptr, new_size] {return tlsf_realloc(ptr, new_size);});
}

static void tlsf_free_wrapped(void * ptr)
{
  pthread_mutex_lock(&tlsf_mtx);
  tlsf_free(ptr);
  pthread_mutex_unlock(&tlsf_mtx);
}

static void * tlsf_aligned_malloc(size_t alignment, size_t size)
{
  void * addr = tlsf_malloc_wrapped(alignment + size);
  void * aligned =
    reinterpret_cast<void *>(reinterpret_cast<uint64_t>(addr) + alignment -
    reinterpret_cast<uint64_t>(addr) % alignment);
  (*aligned2orig)[aligned] = addr;

  //printf("In tlsf_aligned_malloc: orig=%p -> aligned=%p\n", addr, aligned);

  return aligned;
}

using namespace heaphook;

class TlfsAllocator : public GlobalAllocator {
  void *do_alloc(size_t size, size_t alignment) override {
    if (alignment == 1) {
      static malloc_type original_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
      static __thread bool malloc_no_hook = false;

      if (malloc_no_hook) {
        if (mempool_initialized) {
          return tlsf_malloc_wrapped(size);
        } else {
          return original_malloc(size);
        }
      }

      malloc_no_hook = true;
      check_mempool_initialized();
      void * ret = tlsf_malloc_wrapped(size);
      malloc_no_hook = false;
      return ret;
    } else {
      static aligned_alloc_type original_aligned_alloc =
        reinterpret_cast<aligned_alloc_type>(dlsym(RTLD_NEXT, "aligned_alloc"));
      static __thread bool aligned_alloc_no_hook = false;

      if (aligned_alloc_no_hook /*|| pthread_self() == logging_thread*/) {
        if (mempool_initialized) {return tlsf_aligned_malloc(alignment, size);} else {
          return original_aligned_alloc(alignment, size);
        }
      }

      aligned_alloc_no_hook = true;
      check_mempool_initialized();
      void * ret = tlsf_aligned_malloc(alignment, size);
      aligned_alloc_no_hook = false;
      return ret;
    }
  }

  void do_dealloc(void* ptr) override {
    static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
    static __thread bool free_no_hook = false;

    if (free_no_hook) {
      if (mempool_initialized) {
        tlsf_free_wrapped(ptr);
      } else {
        original_free(ptr);
      }

      return;
    }

    free_no_hook = true;
    check_mempool_initialized();

    auto it = aligned2orig->find(ptr);
    if (it != aligned2orig->end()) {
      ptr = it->second;
      aligned2orig->erase(it);
    }

    tlsf_free_wrapped(ptr);
    free_no_hook = false;
  }

  size_t do_get_block_size(void * ptr) override {
    write_to_stderr("\ndo_get_block_size(", ptr, ") is called.\n");
    return 0;
  }

  void *do_realloc(void *ptr, size_t new_size) {
    static realloc_type original_realloc =
      reinterpret_cast<realloc_type>(dlsym(RTLD_NEXT, "realloc"));
    static __thread bool realloc_no_hook = false;

    if (realloc_no_hook) {
      if (mempool_initialized) {
        return tlsf_realloc_wrapped(ptr, new_size);
      } else {
        return original_realloc(ptr, new_size);
      }
    }

    realloc_no_hook = true;
    check_mempool_initialized();

    auto it = aligned2orig->find(ptr);
    if (it != aligned2orig->end()) {
      ptr = it->second;
      aligned2orig->erase(ptr);
    }

    void * ret = tlsf_realloc_wrapped(ptr, new_size);
    realloc_no_hook = false;
    return ret;
  }
};

GlobalAllocator &GlobalAllocator::get_instance() {
  static TlfsAllocator tlfs_allocator;
  return tlfs_allocator;
}
