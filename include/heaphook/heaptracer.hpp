#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <iomanip>
#include <fstream>
#include <mutex>
#include <condition_variable>

#include "utils.hpp"

namespace heaphook
{

struct AllocInfo
{
  size_t bytes;
  size_t align;
  void * retval;
  size_t processing_time;
};

struct DeallocInfo
{
  void * ptr;
  size_t processing_time;
};

struct GetBlockSizeInfo
{
  void * ptr;
  size_t retval;
  size_t processing_time;
};

struct AllocZeroedInfo
{
  size_t bytes;
  void * retval;
  size_t processing_time;
};

struct ReallocInfo
{
  void * ptr;
  size_t new_size;
  void * retval;
  size_t processing_time;
};

// this class designed with singlton design pattern.
class HeapTracer
{
  const static size_t kMaxLogLineLen = 0x400;

  char log_file_name_[0x400];
  int log_file_fd_;
  thread_local static char log_line_buf_[kMaxLogLineLen];

  std::mutex mtx_;

protected:
  HeapTracer();

public:
  HeapTracer(const HeapTracer &) = delete;
  void operator=(const HeapTracer &) = delete;
  HeapTracer(HeapTracer &&) = delete;
  void operator=(HeapTracer &&) = delete;

  static HeapTracer & getInstance()
  {
    static HeapTracer tracer;
    return tracer;
  }

  void write_log(AllocInfo & info)
  {
    // since log_line_buf_ is a thread-local member variable,
    // no mutex is required.
    format_as_csv_entry(
      log_line_buf_, "alloc", info.bytes, info.align, info.retval,
      info.processing_time);
    write_log_line();
  }

  void write_log(DeallocInfo & info)
  {
    format_as_csv_entry(log_line_buf_, "dealloc", info.ptr, info.processing_time);
    write_log_line();
  }

  void write_log(GetBlockSizeInfo & info)
  {
    format_as_csv_entry(
      log_line_buf_, "get_block_size", info.ptr, info.retval,
      info.processing_time);
    write_log_line();
  }

  void write_log(AllocZeroedInfo & info)
  {
    format_as_csv_entry(
      log_line_buf_, "alloc_zeroed", info.bytes, info.retval,
      info.processing_time);
    write_log_line();
  }

  void write_log(ReallocInfo & info)
  {
    format_as_csv_entry(
      log_line_buf_, "realloc", info.ptr, info.new_size, info.retval,
      info.processing_time);
    write_log_line();
  }

private:
  void write_log_line()
  {
    std::unique_lock<std::mutex> lock(mtx_); // is this statement needed ?
    write(log_file_fd_, log_line_buf_, strlen(log_line_buf_));
  }
};

} // namespace heaphook
