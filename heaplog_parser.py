from enum import Enum
import mmap
import sys

import numpy as np
import matplotlib.pyplot as plt
from progress.bar import ShadyBar

heap_history = np.empty(1, dtype=np.int64)
heap_history_num = 1

def read_heap_history(filename):
    global heap_history, heap_history_num

    addr2size = {}

    with open(filename, mode="r", encoding="utf8") as f:
        with mmap.mmap(f.fileno(), length=0, access=mmap.ACCESS_READ) as va:
            line_num = 0
            print("counting the number of log entries...")
            for line in iter(va.readline, b""):
                line_num += 1
            print(line_num, "entries found")

            heap_history = np.empty(line_num + 1, dtype=np.int64)
            heap_history[0] = 0
            key_not_found = 0
            line_idx = 0
            va.seek(0)

            progress_bar = ShadyBar("Progress", max=line_num, suffix="%(percent).1f%% - Elapsed: %(elapsed)ds")

            for line in iter(va.readline, b""):
                hook_type, addr, size = line.rstrip().split()
                addr = int(addr, 16)
                size = int(size, 10)

                if line_idx % 100000 == 0:
                    progress_bar.next(n=100000)
                line_idx += 1

                if (addr == 0):
                    continue

                if hook_type == b'malloc':
                    addr2size[addr] = size
                    heap_history[heap_history_num] = heap_history[heap_history_num - 1] + size
                elif hook_type == b'free':
                    if not addr in addr2size:
                        key_not_found += 1
                        continue

                    heap_history[heap_history_num] = heap_history[heap_history_num - 1] - addr2size[addr]
                    del addr2size[addr]

                heap_history_num += 1

            progress_bar.finish()
            print("key_not_found =", key_not_found)

def visualize():
    fig = plt.figure(figsize=(16, 16))
    ax = fig.add_subplot(1, 1, 1)

    ax.plot(heap_history)
    ax.set_xlabel("Hook index")
    ax.set_ylabel("Accumulated heap allocation size (bytes)")

    plt.show()

if __name__ == "__main__":
    read_heap_history(sys.argv[1])
    heap_history.resize(heap_history_num)

    visualize()

