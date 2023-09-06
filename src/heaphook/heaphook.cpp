#include "heaphook/heaphook.hpp"
#include "heaphook/heaptracer.hpp"
#include "heaphook/utils.hpp"

namespace heaphook {

GlobalAllocator::GlobalAllocator() {
  write_to_stderr("\n💚 heaphook is started 💚\n\n");
}

void *GlobalAllocator::alloc(size_t size, size_t align) {
  if constexpr (HeapTraceEnabled) {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto retval = do_alloc(size, align);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    AllocInfo info { size, align, retval, static_cast<size_t>(duration.count()) };
    HeapTracer::getInstance().write_log(info);
    return retval;
  } else {
    return do_alloc(size, align);
  }
}

void GlobalAllocator::dealloc(void *ptr) {
  if constexpr (HeapTraceEnabled) {
    auto start_time = std::chrono::high_resolution_clock::now();
    do_dealloc(ptr);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    DeallocInfo info { ptr, static_cast<size_t>(duration.count()) };
    HeapTracer::getInstance().write_log(info);
  } else {
    do_dealloc(ptr);
  }
}

size_t GlobalAllocator::get_block_size(void *ptr) {
  if constexpr (HeapTraceEnabled) {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto retval = do_get_block_size(ptr);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    GetBlockSizeInfo info { ptr, retval, static_cast<size_t>(duration.count()) };
    HeapTracer::getInstance().write_log(info);
    return retval;
  } else {
    return do_get_block_size(ptr);
  }
}

void *GlobalAllocator::alloc_zeroed(size_t size) {
  if constexpr (HeapTraceEnabled) {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto retval = do_alloc_zeroed(size);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    AllocZeroedInfo info { size, retval, static_cast<size_t>(duration.count()) };
    HeapTracer::getInstance().write_log(info);
    return retval;
  } else {
    return do_alloc_zeroed(size);
  }
}

void *GlobalAllocator::realloc(void *ptr, size_t new_size) {
  if constexpr (HeapTraceEnabled) {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto retval = do_realloc(ptr, new_size);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    ReallocInfo info { ptr, new_size, retval, static_cast<size_t>(duration.count()) };
    HeapTracer::getInstance().write_log(info);
    return retval;
  } else {
    return do_realloc(ptr, new_size);
  }
}

void *GlobalAllocator::do_alloc_zeroed(size_t size) {
  auto retval = do_alloc(size, 1);
  if (retval == nullptr) {
    return nullptr;
  }
  memset(retval, 0, size);
  return retval;
}

void *GlobalAllocator::do_realloc(void *ptr, size_t new_size) {
  auto old_size = do_get_block_size(ptr);
  if (new_size <= old_size) {
    return ptr;
  }
  void *retval = do_alloc(new_size, 1);
  if (retval == nullptr)
    return nullptr;
  memcpy(retval, ptr, old_size);
  do_dealloc(ptr);
  return retval;
}

} // namespace heaphook