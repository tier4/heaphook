#include <dlfcn.h>
#include <malloc.h>
#include <pthread.h>
#include <cstdlib>
#include <string>

#include "heaphook/heaphook.hpp"
#include "heaphook/hook_types.hpp"

static size_t INITIAL_SBRK_SIZE = 1000 * 1000 * 1000; // default: 1GB
static size_t MMAP_THRESHOLD = 100 * 1000 * 1000; // default: 100MB

static pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t init_cond = PTHREAD_COND_INITIALIZER; // mempool_initialized == true
static bool mempool_initialized = false;
static bool mempool_init_started = false; // guarded by init_mtx

static void initialize_mempool()
{
  if (const char * env_p = std::getenv("INITIAL_SBRK_SIZE")) {
    INITIAL_SBRK_SIZE = std::stoull(std::string(env_p));
  }

  if (const char * env_p = std::getenv("MMAP_THRESHOLD")) {
    MMAP_THRESHOLD = std::stoull(std::string(env_p));
  }

  char* p = static_cast<char*>(sbrk(INITIAL_SBRK_SIZE));
  if (p == (char*)-1) {
    // TODO: error handling
    return;
  }

  memset(p, 0, INITIAL_SBRK_SIZE);

  mallopt(MMAP_THRESHOLD, MMAP_THRESHOLD);
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

void write_error_string(const char * s)
{
  write(STDERR_FILENO, s, strlen(s));
}

void perror(const char * s)
{
  write_error_string(s);
  exit(-1);
}

using namespace heaphook;

// this allocator transfers directly to GLIBC functions.
class OriginalAllocator : public GlobalAllocator
{
  void * do_alloc(size_t bytes, size_t align) override
  {
    if (align == 1) {
      static malloc_type original_malloc =
        reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));

      check_mempool_initialized();
      return original_malloc(bytes);
    } else {
      static memalign_type original_memalign =
        reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));

      check_mempool_initialized();
      return original_memalign(align, bytes);
    }
  }

  void do_dealloc(void * ptr) override
  {
    static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));

    check_mempool_initialized();
    original_free(ptr);
  }

  size_t do_get_block_size(void * ptr) override
  {
    static malloc_usable_size_type original_malloc_usable_size = \
      reinterpret_cast<malloc_usable_size_type>(dlsym(RTLD_NEXT, "malloc_usable_size"));

    check_mempool_initialized();
    return original_malloc_usable_size(ptr);
  }

  void * do_alloc_zeroed(size_t bytes) override
  {
    static calloc_type original_calloc = reinterpret_cast<calloc_type>(dlsym(RTLD_NEXT, "calloc"));

    check_mempool_initialized();
    return original_calloc(bytes, 1);
  }

  void * do_realloc(void * ptr, size_t new_size)
  {
    static realloc_type original_realloc = reinterpret_cast<realloc_type>(dlsym(
        RTLD_NEXT,
        "realloc"));

    check_mempool_initialized();
    return original_realloc(ptr, new_size);
  }
};

GlobalAllocator & GlobalAllocator::get_instance()
{
  static OriginalAllocator original;
  return original;
}
