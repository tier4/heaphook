# heaphook
Replace all the dynamic heap allocation functions by LD_PRELOAD.

## Build and Install
This library `heaphook` is prepared as a `ament_cmake` package.
```
$ mkdir -p /path/to/heaphook_ws && cd /path/to/heaphook_ws
$ mkdir src & git clone git@github.com:tier4/heaphook.git src
$ colcon build
```
The shared libraries that will be specified in `LD_PRELOAD` are generated under `install/heaphook/lib`.
This path is added to `LD_LIBRARY_PATH` by the setup script.
```
$ source install/setup.bash
```

## How to use
For now, We provide two kinds of the heaphook libraries.
- `libpreload_heaptrack.so`: Records all the heap allocation/deallocation function calls and generate a log file for visualizing the history of heap consumption.
- `libpreloaded_tlsf.so`: Replaces all the heap allocation/deallocation with TLSF (Tow-Level Segregated Fit) memory allocator.

A typical use case is to utilize `libpreloaded_heaptrack` to grasp the transition and maximum value of heap consumtion of the target process
and to determine the initial allocated memory pool size for `libpreloaded_tlsf`.
Of cource, it is also useful to utilize `libpreloaded_heaptrack` just to know the heap consumption of the target process.

### libpreloaded_heaptrack
You can track the heap consumption of the specified process.
```
$ LD_PRELOAD=libpreloaded_heaptrack.so executable
```
A log file is generated under the current working directory in the format `heaplog.{%pid}.log`.
You can visualize heap consumption transitions in PDF format based on the generated log file.
The parser depends on `progressbar` python library, so install it before.
```
$ pip install progressbar
$ python3 heaplog_parser.py heaplog.{%pid}.log // Generates heaplog.{%pid}.pdf
```


### libpreloaded_tlsf
You need to specify the initial allocaton size for the memory pool and the size of additional memory to be allocated
when the initial memory pool size is not sufficient.
The initial size of the memory pool should be set to a value with a margin greater than the peak heap usage of the target process.
Extending the memory pool size during the runtime should be avoided, as it leads to overhead in the allocation functions.
```
$ LD_PRELOAD=libpreloaded_tlsf.so INITIAL_MEMPOOL_SIZE=1000000 ADDITIONAL_MEMPOOL_SIZE=1000000 executable
```

When the initial size of the memory pool is insufficient, it first allocates the additional size specified by `ADDITIONAL_MEMPOOL_SIZE`,
and if it is still insufficient, it allocates double the value of the previous allocation until it is sufficient.

As an example, here is what happens when `malloc(1000)` is called when the existing memory pool is exhausted and `ADDITIONAL_MEMPOOL_SIZE=100`.
1. 100 bytes added to the memory pool (still not sufficient)
2. 200 bytes added to the memory pool (still not sufficient)
3. 400 bytes added to the memory pool (still not sufficient)
4. 800 bytes added to the memory pool (still not sufficient)
5. 1600 bytes added to the memory pool (finally sufficient)

The added memory pool areas are not contiguous with each other in the virual address space,
so it is not necessarily enough even if the total size of the added memory pools exceeds the size of the memory allocation request. 

## Integrate with ROS2 launch
You can easily integrate `heaphook` with ROS2 launch systems.
From the launch file, you can replace all heap allocations of the process corresponding to the targeted `Node` and `ComposableNodeContainer`.

```xml
<node pkg="..." exec="..." name="...">
  <env name="LD_PRELOAD" value="libpreloaded_heaptrack.so" />
</node>
```

```xml
<node pkg="..." exec="..." name="...">
  <env name="LD_PRELOAD" value="libpreloaded_tlsf.so />
  <env name="INITIAL_MEMPOOL_SIZE" value="100000000" />
  <env name="ADDITIONAL_MEMPOOL_SIZE" value="100000000" />
</node>
```

```python
container = ComposableNodeContainer(
  ...,
  additional_env={"LD_PRELOAD": "libpreloaded_heaptrack.so"},
)
```

```python
container = ComposableNodeContainer(
  ...,
  additional_env={
    "LD_PRELOAD": "libpreloaded_tlsf.so",
    "INITIAL_MEMPOOL_SIZE": "100000000", # 100MB
    "ADDITIONAL_MEMPOOL_SIZE": "100000000",
  },
)
```

### liboriginal_allocator.so
```
$ LD_PRELOAD=liboriginal_allocator.so executable
```
This allocator uses the GLIBC memory allocator internally.

## How to add a new memory allocator
If you want to implement an allocator to replace the GLIBC memory allcator, you can easily do so by using this heaphook library.

The steps you will take are as follows.
### 1. Create source file
What you have to implement are
* Include `heaphook/heaphook.hpp` header file.
* Implement your own allocator class that inherits the abstract base class `GlobalAllocator` defined in `heaphook/heaphook.hpp`.
  * This base class has 5 virtual functions: `do_alloc`, `do_dealloc`, `do_alloc_zeroed`, `do_realloc` and `do_get_block_size`.
  * `do_alloc_zeroed` and `do_realloc` has default implementation, so you don't have to implement them.
  * For more information on the GlobalAllocagor API, see here.
* Implement static member function named `get_instance` in `GlobalAllocator`.
  * The implementation of this static member function is almost a fixed form. It defines its own allocator as a static local variable and returns a reference to its instance.
  * See the following example for a concrete implementation.


The minimum implementation required is as follows.
```cpp
#include "heaphook/heaphook.hpp"
using namespace heaphook;

class MyAllocator : public GlobalAllocator {
  void* do_alloc(size_t size, size_t align) override {
    ...
  }

  void do_dealloc(void* ptr) override {
    ...
  }

  size_t do_get_block_size(void * ptr) override {
    ...
  }
};

GlobalAllocator &GlobalAllocator::get_instance() {
  static MyAllocator allocator;
  return allocator;
}
```
Save the above implementation in src/my_allocator.cpp.

### 2. Edit CMakeLists.txt
To build the implemented allocator, you must edit CMakeLists.txt.
You can use build_library cmake function to add build target.

build_library takes a library name as its first argument and a set of required source files as the second and subsequent arguments.

For example, 
```cmake
build_library(my_allocator
  src/my_allocator.cpp)
```

### 3. Build
Go to the top directory and execute the following command.
```shell
$ colcon build
```
If the build is successful, a file named `lib<libname>.so` should be created.
You can use it as follows.
```shell
$ source install/setup.bash
$ LD_PRELOAD=libmy_allocator.so executable
```

## heaphook API
This section describes the `GlobalAllocator` class, which is required when creating an allocator.

The `GlobalAllocator` class has 5 virtual member functions.
### do_alloc
```cpp
void *GlobalAllocator::do_alloc(size_t size, size_t align);
``` 
This function allocates a memory area that is larger than `size` byte and aligned with `align` and returns its address. When implementing this function, the following conditions can be assumed. (heaphook will filter out those that do not meet these conditions.)
* `size` > 0
* `align` is either
  * 1, which means there are no alignment constraints.
  * a power of 2 and multiple of `sizeof(void *)`, such as 8, 16, 32, ..., 4096, ...

If memory allocation fails due to various factors, `nullptr` should be returned.

This function hooks the GLIBC allocation functions `malloc`, `posix_memalign`, `memalign`, `aligned_alloc`, `valloc` and `pvalloc`.

### do_dealloc
```cpp
void GlobalAllocator::do_dealloc(void *ptr);
```
This function deallocates a memory area pointed to by ptr, which must have been allocated by other allocation functions `do_alloc`, `do_alloc_zeroed` or `do_realloc`. The following conditions can be assumed,
* `ptr` is the value previously returned from these allocation functions. (If not, the program may be killed.)
* `ptr` != `nullptr`

This function hooks the `free` function in GLIBC.

### do_alloc_zeroed
```cpp
void *GlobalAllocator::do_alloc_zeroed(size_t size);
``` 
This function allocates a memory area that is larger than `size` byte. The memory is set to zero. The following conditions can be assumed,
* `size` > 0

If memory allocation fails due to various factors, `nullptr` should be returned.

This function hooks the `calloc` function in GLIBC.

A default implementation is provided that calls `do_alloc` and initializes the allocated memory area with zeros using `memset`.

### do_realloc
```cpp
void *GlobalAllocator::do_realloc(void *ptr, size_t new_size);
``` 
This function changes the size of the memory block pointed to by `ptr` to `new_size` bytes and return a pointer to the new memory area. The contents of the memory area must remain unchanged. When expanging the memory, the added memory does not need to be initialized. The following conditions can be assumed,
* `ptr` is the value previously returned from these allocation functions. (If not, the program may be killed.)
* `ptr` != `nullptr`
* `new_size` > 0

If memory allocation fails due to various factors, `nullptr` should be returned.

This function hooks the `realloc` function in GLIBC.

A default implementation is provided that performs `do_alloc(new_size, 1)`, copies contents, and then deallocates the original pointer using `dealloc(ptr)`.

### do_get_block_size
```cpp
void *GlobalAllocator::do_get_block_size(void *ptr);
``` 
This function returns the size of the memory block pointed to by `ptr`. The following conditions can be assumed,
* `ptr` is the value previously returned from these allocation functions. (If not, the program may be killed.)
* `ptr` != `nullptr`

This function is called internally when calling the GLIBC's `malloc_usable_size`.

## Trace function
heaphook has a trace function for debugging the allocator and analyzing its performance.

In the CMakeList.txt file, after declaring a new allocator building rule with `build_library`, you can specify `target_compile_options(<libname> PRIVATE "-DTRACE")` to build library that traces the allocator's behavior.
```cmake
build_library(my_allocator ...)
target_compile_options(my_allocator PRIVATE "-DTRACE")
```
If configured in this way, the library will generate a log file named `heaplog.<pid>.log` in the current directory. You can visualize heap consumption transitions and performance of each GlobalAllocator member functions in png format based on the generated log file.
```bash
$ misc/heaptrace_analyzer.py heaplog.<pid>.log 
```

## Test allocator
TODO:
