#include <vector>
#include <iostream>
#include <stdlib.h>
#include <malloc.h>

int main()
{
  {
    std::vector<int> v;
    v.resize(200);
  }

  std::vector<char> v2;
  v2.resize(100);

  char * buffer = (char *) malloc(100);
  buffer = (char *) malloc(400);
  (void) buffer;

  int * value = new int(0);
  delete value;

  int * values = new int[10];
  delete[] values;

  {
    void * ptr;
    int ret = posix_memalign(&ptr, 4096, 1111);
    if (ret != 0) {
      perror("posix_memalign error");
    }

    std::cout << "In user program: posix_memalign allocated at " << ptr << std::endl;

    free(ptr);
    std::cout << "In user program: free(" << ptr << ")" << std::endl;
  }

  {
    void * ptr;
    int ret = posix_memalign(&ptr, 32, 1111);
    if (ret != 0) {
      perror("posix_memalign error");
    }

    std::cout << "In user program: posix_memalign allocated at " << ptr << std::endl;

    void * ptr2 = realloc(ptr, 1212);
    std::cout << "In user program: re-allocated at " << ptr2 << std::endl;
  }

  {
    void * addr = memalign(4096, 2222);
    std::cout << "memalign allocated at " << addr << std::endl;
  }

  {
    // Should be error by specification, but valid in some implementation.
    void * addr = aligned_alloc(4096, 3333);
    std::cout << "aligned_alloc allocated at " << addr << std::endl;
  }

  {
    void * addr = valloc(4444);
    std::cout << "valloc allocated at " << addr << std::endl;
  }

  {
    void * addr = pvalloc(5555);
    std::cout << "pvalloc allocated at " << addr << std::endl;

    size_t size = malloc_usable_size(addr);
    std::cout << "malloc_usable_size() returns " << size << std::endl;
  }
}
