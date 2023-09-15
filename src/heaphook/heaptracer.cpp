#include "heaphook/heaptracer.hpp"

namespace heaphook
{

HeapTracer::HeapTracer()
{
  format(log_file_name_, "./heaplog_", getpid(), ".log");
  log_file_fd_ = open(log_file_name_, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (log_file_fd_ == -1) {
    write_to_stderr("\n[ heaphook::HeapTracer ] ERROR: failed to open log file.\n");
    exit(-1);
  }
}

thread_local char HeapTracer::log_line_buf_[HeapTracer::kMaxLogLineLen];

} // namespace heaphook
