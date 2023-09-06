#include "heaphook/heaptracer.hpp"

namespace heaphook {

HeapTracer::HeapTracer() {
  write_to_stderr("\n😺 HeapTracer 😺\n\n");
  format(log_file_name_, "./", getpid(), ".log");
  log_file_fd_ = open(log_file_name_, O_WRONLY|O_CREAT|O_TRUNC, 0666);
  if (log_file_fd_ == -1) {
    write(STDERR_FILENO, "\n[ heaphook ] ERROR: failed to open log file.\n", 47);
    exit(-1);
  }
}

thread_local char HeapTracer::log_line_buf_[HeapTracer::kMaxLogLineLen];

} // namespace heaphook