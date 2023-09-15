#include "heaphook/heaphook.hpp"
using namespace heaphook;

// This allocator is for illustrative purposes only.
// It tries to do nothing.
class MyAllocator : public GlobalAllocator
{
  void * do_alloc(size_t bytes, size_t align) override
  {
    return nullptr;
  }

  void do_dealloc(void * ptr) override
  {

  }

  size_t do_get_block_size(void * ptr) override
  {
    return 0;
  }
};

GlobalAllocator & GlobalAllocator::get_instance()
{
  static MyAllocator allocator;
  return allocator;
}
