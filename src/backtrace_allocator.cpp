#include "heaphook/heaphook.hpp"
#include "heaphook/hook_types.hpp"

#include <dlfcn.h>
#include <execinfo.h>

#include <array>
#include <memory_resource>
#include <queue>
#include <unordered_map>

using namespace heaphook;

// This allocator shows the stack traces when malloc/new is called.
// The purpose is to find out indirect memory allocation introduced by
// high-level C++ functions. Then we can try to remove them to reduce the
// time spent in page faults.

// Many C++ features (std::cout, std::string) use malloc/new
// internally. We should avoid using them as much as possible.
// When the implementation needs memory, it's better to use stack or data segment.

constexpr size_t MAX_NUM_BACKTRACE_FRAMES = 32;
thread_local static int g_use_vanilla_allocator = 0;

struct BackTrace
{
  void * frame_ptrs_[MAX_NUM_BACKTRACE_FRAMES];
  int num_frames_;

  size_t hash() const
  {
    size_t hash = 0;
    for (int i = 0; i < num_frames_; i++) {
      hash = (hash << 1) ^ reinterpret_cast<size_t>(frame_ptrs_[i]);
    }
    return hash;
  }

  bool operator==(const BackTrace & other) const { return hash() == other.hash(); }

  void repr_as(char * buf, size_t buf_size) const
  {
    char ** symbols = backtrace_symbols(frame_ptrs_, num_frames_);
    size_t pos = 0;
    for (int i = 0; i < num_frames_; i++) {
      pos += snprintf(buf + pos, buf_size - pos, "%s\n", symbols[i]);
    }
    free(symbols);
  }
};

struct AllocRecord
{
  size_t bytes_;
  size_t num_calls_;

  AllocRecord() = default;
  AllocRecord(size_t bytes) : bytes_(bytes), num_calls_(1) {}

  void inc_amount(size_t delta)
  {
    bytes_ += delta;
    num_calls_ += 1;
  }
};

namespace std
{

template <>
struct hash<BackTrace>
{
  std::size_t operator()(const BackTrace & bt) const { return bt.hash(); }
};

}  // namespace std

class VanillaAllocator : public GlobalAllocator
{
  void * do_alloc(size_t bytes, size_t align) override
  {
    if (align == 1) {
      static malloc_type original_malloc =
        reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
      return original_malloc(bytes);
    } else {
      static memalign_type original_memalign =
        reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));
      return original_memalign(align, bytes);
    }
  }

  void do_dealloc(void * ptr) override
  {
    static free_type original_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
    original_free(ptr);
  }

  size_t do_get_block_size(void * ptr) override
  {
    static malloc_usable_size_type original_malloc_usable_size =
      reinterpret_cast<malloc_usable_size_type>(dlsym(RTLD_NEXT, "malloc_usable_size"));
    return original_malloc_usable_size(ptr);
  }
};

class BacktraceAllocator : public VanillaAllocator
{
private:
  std::pmr::monotonic_buffer_resource buf_resource_;
  std::pmr::unordered_map<BackTrace, AllocRecord> alloc_records_;
  std::array<std::byte, 64 * 1024 * 1024> mem_buf_;

  void * do_alloc(size_t bytes, size_t align) override
  {
    g_use_vanilla_allocator = 1;
    struct BackTrace bt;
    bt.num_frames_ = backtrace(bt.frame_ptrs_, MAX_NUM_BACKTRACE_FRAMES);

    auto it = alloc_records_.find(bt);
    if (it != alloc_records_.end()) {
      it->second.inc_amount(bytes);
    } else {
      alloc_records_[bt] = AllocRecord(bytes);
    }
    g_use_vanilla_allocator = 0;

    if (align == 1) {
      static malloc_type original_malloc =
        reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
      return original_malloc(bytes);
    } else {
      static memalign_type original_memalign =
        reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));
      return original_memalign(align, bytes);
    }
  }

  int save_top_allocs(const char * output_filename, const bool bytes_based)
  {
    FILE * fp = fopen(output_filename, "w");
    char line[1024];
    if (!fp) {
      snprintf(line, sizeof(line), "Fail to write %s", output_filename);
      puts(line);
      return 1;
    } else {
      snprintf(line, sizeof(line), "Write %s", output_filename);
      puts(line);
    }

    // Show only |num_tops| callers
    size_t num_tops = 10;
    if (getenv("NUM_TOPS")) {
      num_tops = atol(getenv("NUM_TOPS"));
    }

    // We usually care about recurrent calls. If a call site just make one malloc,
    // it is not usually the target that we want to optimize away;
    bool show_recurrent_callers = false;
    if (getenv("SHOW_RECURRENT_CALLERS") && getenv("SHOW_RECURRENT_CALLERS")[0] == '1') {
      show_recurrent_callers = true;
    }

    using bt_record_pair_t = std::pair<BackTrace, AllocRecord>;
    using compare_t = bool (*)(const bt_record_pair_t &, const bt_record_pair_t &);

    compare_t cmp;

    if (bytes_based) {
      cmp = [](const bt_record_pair_t & a, const bt_record_pair_t & b) {
        return a.second.bytes_ > b.second.bytes_;
      };
    } else {
      cmp = [](const bt_record_pair_t & a, const bt_record_pair_t & b) {
        return a.second.num_calls_ > b.second.num_calls_;
      };
    }

    std::priority_queue<bt_record_pair_t, std::vector<bt_record_pair_t>, decltype(cmp)> min_pq(cmp);

    for (const auto & [bt, record] : alloc_records_) {
      if (show_recurrent_callers || record.num_calls_ > 1) {
        min_pq.push(bt_record_pair_t{bt, record});
        while (min_pq.size() > num_tops) {
          min_pq.pop();
        }
      }
    }

    bool is_first_write = true;
    while (!min_pq.empty()) {
      auto e = min_pq.top();
      auto & bt = e.first;
      auto & record = e.second;
      if (!is_first_write) {
        fputs("\n", fp);
      }
      is_first_write = false;

      snprintf(
        line, sizeof(line), "Allocate %ld bytes with %ld calls:\n", record.bytes_,
        record.num_calls_);
      fputs(line, fp);

      char buf[4096] = {0};
      bt.repr_as(buf, sizeof(buf));
      fputs(buf, fp);

      min_pq.pop();
    }
    fclose(fp);
    return 0;
  }

public:
  BacktraceAllocator() : buf_resource_{mem_buf_.data(), mem_buf_.size(), std::pmr::null_memory_resource()}, alloc_records_(&buf_resource_)
  {
  }

  ~BacktraceAllocator()
  {
    g_use_vanilla_allocator = 1;

    size_t total_bytes = 0;
    size_t total_num_calls = 0;
    int num_items = alloc_records_.size();
    for (const auto & [bt, record] : alloc_records_) {
      total_bytes += record.bytes_;
      total_num_calls += record.num_calls_;
      num_items--;
      if (num_items <= 0) {
        // gcc-bug: The ranged-for loop can become an infinite loop in rare cases, use counter to break out.
        break;
      }
    }

    {
      char line[1024] = {0};
      snprintf(
        line, sizeof(line), "%lu backtraces: allocate %lu bytes with %lu malloc/new calls.",
        alloc_records_.size(), total_bytes, total_num_calls);
      puts(line);
    }

    {
      char output_filename[64] = {0};
      snprintf(output_filename, sizeof(output_filename), "top_alloc_bytes_bt.%d.%d.log", getpid(), gettid());
      save_top_allocs(output_filename, true);

      snprintf(output_filename, sizeof(output_filename), "top_num_calls_bt.%d.%d.log", getpid(), gettid());
      save_top_allocs(output_filename, false);
    }
    g_use_vanilla_allocator = 0;
  }
};

GlobalAllocator & GlobalAllocator::get_instance()
{
  static VanillaAllocator vanilla_allocator;
  static BacktraceAllocator backtrace_allocator;
  if (g_use_vanilla_allocator) {
    return vanilla_allocator;
  } else {
    return backtrace_allocator;
  }
}
