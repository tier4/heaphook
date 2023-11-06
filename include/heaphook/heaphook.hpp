#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <unistd.h>
#include <cstring>

#include <chrono>

#ifdef TRACE
static constexpr bool HeapTraceEnabled = true;
#else
static constexpr bool HeapTraceEnabled = false;
#endif

namespace heaphook
{

// this class designed with singlton design pattern.
class GlobalAllocator
{
protected:
  // users cannot use default constructor
  GlobalAllocator();

public:
  // uncopyable and unmovable
  GlobalAllocator(const GlobalAllocator &) = delete;
  GlobalAllocator & operator=(const GlobalAllocator &) = delete;
  GlobalAllocator(GlobalAllocator &&) = delete;
  GlobalAllocator & operator=(GlobalAllocator &&) = delete;

  // Users must implement this method as follows
  //
  // static GlobalAllocator &get_instance() {
  //   static DerivedAllocator allocator;
  //   return allocator;
  // }
  static GlobalAllocator & get_instance();

  // alloc allocates size bytes and return base address.
  // alloc is called from mallo, aligned_alloc, etc..
  //
  // size is a positive value.
  //
  // if no alignment is specified, it defaults to 1.
  // if specified, it is a value satisfying the following conditions
  //
  // * align is a power of 2
  // * align is a multiple of sizeof(void*)
  //
  // if allocation fails, returns nullptr.
  [[nodiscard]]
  void * alloc(size_t size, size_t align = 1);

  // this member function releases memory areas allocated
  // by alloc, alloc_zeroed, and realloc.
  //
  // ptr is not nullptr and must be the value returned by those functions.
  void dealloc(void * ptr);

  // this function returns the size of the memory area pointed to by ptr.
  size_t get_block_size(void * ptr);

  // this function similar to alloc,
  // differing only in that it initializes the allocated area with 0.
  //
  // size > 0
  //
  // if allocation fails, returns nullptr.
  [[nodiscard]]
  void * alloc_zeroed(size_t size);

  // this function resizes the allocated area
  //
  // new_size > 0
  // ptr != nullptr
  //
  // if allocation fails, returns nullptr.
  [[nodiscard]]
  void * realloc(void * ptr, size_t new_size);

private:
  virtual void * do_alloc(size_t, size_t) = 0;

  virtual void do_dealloc(void *) = 0;

  virtual size_t do_get_block_size(void *) = 0;

  // this member function has default implementation
  virtual void * do_alloc_zeroed(size_t size);

  // this member function has default implementation
  virtual void * do_realloc(void * ptr, size_t new_size);
};

} // namespace heaphook
