#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <malloc.h>

#include <cstdint>
#include <unordered_map>
#include <string>
#include <memory>

#include "tlsf/tlsf.h"

#include "heaphook/heaphook.hpp"
#include "heaphook/hook_types.hpp"
#include "heaphook/utils.hpp"

static char * mempool_ptr;
static size_t INITIAL_MEMPOOL_SIZE = 100 * 1000 * 1000; // default: 100MB
static size_t ADDITIONAL_MEMPOOL_SIZE = 50 * 1000 * 1000; // default: 50MB

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
}

// Do not use printf
void check_mempool_initialized()
{
  if (mempool_initialized) return;

  pthread_mutex_lock(&init_mtx);

  if (!mempool_init_started) {
    mempool_init_started = true;
    pthread_mutex_unlock(&init_mtx);

    initialize_mempool();

    mempool_initialized = true;
    pthread_cond_signal(&init_cond);
  } else {
    while (!mempool_initialized) {
      pthread_cond_wait(&init_cond, &init_mtx);
    }

    pthread_mutex_unlock(&init_mtx);
  }
}

static void * tlsf_malloc_wrapped(size_t size)
{
  pthread_mutex_lock(&tlsf_mtx);
  void * ret = tlsf_malloc(size);
  pthread_mutex_unlock(&tlsf_mtx);

  size_t multiplier = 1;
  while (ret == NULL) {
    char * addr = (char *) mmap(
      NULL, multiplier * ADDITIONAL_MEMPOOL_SIZE, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    pthread_mutex_lock(&tlsf_mtx);
    add_new_area(addr, multiplier * ADDITIONAL_MEMPOOL_SIZE, mempool_ptr); // tlsf library function
    pthread_mutex_unlock(&tlsf_mtx);

    fprintf(
      stderr, "TLSF memory pool exhausted: %lu bytes additionally mmaped.\n",
      multiplier * ADDITIONAL_MEMPOOL_SIZE);

    pthread_mutex_lock(&tlsf_mtx);
    ret = tlsf_malloc(size);;
    pthread_mutex_unlock(&tlsf_mtx);

    multiplier *= 2;
  }

  return ret;
}

static void tlsf_free_wrapped(void * ptr)
{
  void** p = reinterpret_cast<void**>(ptr);

  pthread_mutex_lock(&tlsf_mtx);
  tlsf_free(p[-1]);
  pthread_mutex_unlock(&tlsf_mtx);
}

static void * tlsf_aligned_malloc(size_t alignment, size_t size)
{
  size_t total_size = size + alignment + sizeof(void*) + sizeof(size_t);
  void* raw_memory = tlsf_malloc_wrapped(total_size);

  void* aligned_memory = static_cast<char*>(raw_memory) + sizeof(void*) + sizeof(size_t);
  size_t remain = total_size - sizeof(void*) + sizeof(size_t);
  void* result = std::align(alignment, size, aligned_memory /* updated */, remain /* updated */);

  if (!result) {
    tlsf_free_wrapped(raw_memory);
    throw std::bad_alloc();
  }

  void** p = reinterpret_cast<void**>(aligned_memory);
  size_t* p2 = reinterpret_cast<size_t*>(p - 1);
  p[-1] = raw_memory;
  p2[-1] = alignment;

  //printf("In tlsf_aligned_malloc: orig=%p -> aligned=%p\n", addr, aligned);

  return aligned_memory;
}

using namespace heaphook;

class TlfsAllocator : public GlobalAllocator
{
  void * do_alloc(size_t size, size_t alignment) override
  {
    static malloc_type original_malloc =
        reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
    static aligned_alloc_type original_aligned_alloc =
        reinterpret_cast<aligned_alloc_type>(dlsym(RTLD_NEXT, "aligned_alloc"));

    static __thread bool malloc_no_hook = false;

    if (malloc_no_hook) {
      if (mempool_initialized) {
        return tlsf_aligned_malloc(alignment, size);
      } else {
        return alignment == 1 ? original_malloc(size) : original_aligned_alloc(alignment, size);
      }
    }

    malloc_no_hook = true;
    check_mempool_initialized();
    void * ret = tlsf_aligned_malloc(alignment, size);
    malloc_no_hook = false;
    return ret;
  }

  void do_dealloc(void * ptr) override
  {
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
    tlsf_free_wrapped(ptr);
    free_no_hook = false;
  }

  size_t do_get_block_size(void * ptr) override
  {
    //               |--------------------|
    //               |      prev_hdr      |
    //               |--------------------|
    //               |        size      |U|
    //               |--------------------| -+
    // block_ptr --> |                    |  |
    //               |--------------------|  | unused size
    //               |                    |  |
    //               |--------------------| -+
    //   buf_ptr --> |       buffer       |
    //               |--------------------|

    void ** p = reinterpret_cast<void**>(ptr);
    void* block_ptr = p[-1];
    size_t block_addr = reinterpret_cast<size_t>(block_ptr);

    size_t* p2 = reinterpret_cast<size_t*>(p - 1);
    size_t alignment = p2[-1];

    size_t block_size = (*reinterpret_cast<size_t *>(block_addr - 8)) & (~0b1111ull);
    return block_size - sizeof(void*) - sizeof(size_t) - alignment;
  }
};

GlobalAllocator & GlobalAllocator::get_instance()
{
  static TlfsAllocator tlfs_allocator;
  return tlfs_allocator;
}
