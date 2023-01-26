#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

const char* map_file(const char *name, size_t &len_out) {
  int fd = open(name, O_RDONLY);
  if (fd == -1) {
    perror("open error");
    exit(EXIT_FAILURE);
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    perror("fstat error");
    exit(EXIT_FAILURE);
  }

  len_out = st.st_size;

  const char *addr = static_cast<char*>(mmap(NULL, len_out, PROT_READ, MAP_PRIVATE, fd, 0));
  if (addr == MAP_FAILED) {
    perror("mmap error");
    exit(EXIT_FAILURE);
  }

  return addr;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "argc must be 2" << std::endl;
    return 0;
  }

  size_t file_size;
  const char *mapped_addr = map_file(argv[1], file_size);
  const char *ptr = mapped_addr;
  const char *end_ptr = ptr + file_size;

  unsigned long long mem_sum = 0;
  unsigned long long mem_sum_mx = 0;
  int skip_num = 0;
  int line_num = 0;
  std::unordered_map<void*, int> addr2size;
  addr2size.reserve(1000000);

  size_t read_size = 0;
  while (read_size < file_size) {
    const char *new_line = static_cast<const char*>(memchr(ptr, '\n', end_ptr - ptr));
    const std::string line(ptr, new_line - ptr);
    std::istringstream iss(line);
    read_size += new_line - ptr + 1;
    ptr = new_line + 1;

    std::string hook_type;
    size_t size;
    void *addr;
    iss >> hook_type >> addr >> size;

    if (hook_type == "free") {
      if (addr2size.find(addr) == addr2size.end()) {
        skip_num++;
        continue;
      }

      /* if (addr2size[addr] < 10000) */ mem_sum -= addr2size[addr];
      addr2size.erase(addr);
    } else if (hook_type == "malloc") {
      addr2size[addr] = size;
      /* if (size < 10000) */ mem_sum += size;
    }

    mem_sum_mx = std::max(mem_sum_mx, mem_sum);

    if (mem_sum_mx > 10e18) {
      std::cout << line << std::endl;
      break;
    }

    if (line_num % 100000 == 0) {
      std::cout << line_num << ": " << mem_sum_mx << std::endl;
    }

    line_num++;
  }

  std::cout << "mem_sum_mx = " << mem_sum_mx << std::endl;
  std::cout << "skip_num = " << skip_num << std::endl;
}

