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

