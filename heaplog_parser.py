import mmap
import os
import sys

import matplotlib.pyplot as plt
import numpy as np
from progress.bar import ShadyBar

heap_history = np.empty(1, dtype=np.int64)
heap_history_num = 1


def read_heap_history(filename):
    global heap_history, heap_history_num

    addr2size = {}

    with open(filename, mode='r', encoding='utf8') as f:
        with mmap.mmap(f.fileno(), length=0, access=mmap.ACCESS_READ) as va:
            line_num = 0
            print('counting the number of log entries...')
            for line in iter(va.readline, b''):
                line_num += 1
            print(line_num, 'entries found')

            heap_history = np.empty(line_num + 1, dtype=np.int64)
            heap_history[0] = 0

            key_not_found = 0
            line_idx = 0

            malloc_num = 0
            free_num = 0
            calloc_num = 0
            realloc_num = 0
            posix_memalign_num = 0
            memalign_num = 0
            aligned_alloc_num = 0
            valloc_num = 0
            pvalloc_num = 0
            malloc_usable_size_num = 0

            va.seek(0)

            progress_bar = ShadyBar(
                'Progress',
                max=line_num,
                suffix='%(percent).1f%% - Elapsed: %(elapsed)ds')

            for line in iter(va.readline, b''):
                hook_type, addr, size, new_addr = line.rstrip().split()
                addr = int(addr, 16)
                size = int(size, 10)
                new_addr = int(new_addr, 16)

                if line_idx % 100000 == 0:
                    progress_bar.next(n=100000)
                line_idx += 1

                if (addr == 0):
                    continue

                if hook_type in [
                    b'malloc', b'calloc', b'posix_memalign', b'memalign',
                    b'aligned_alloc', b'valloc', b'pvalloc'
                ]:
                    addr2size[addr] = size
                    heap_history[heap_history_num] = heap_history[heap_history_num - 1] + size

                    if hook_type == b'malloc':
                        malloc_num += 1
                    elif hook_type == b'calloc':
                        calloc_num += 1
                    elif hook_type == b'posix_memalign':
                        posix_memalign_num += 1
                    elif hook_type == b'memalign':
                        memalign_num += 1
                    elif hook_type == b'aligned_alloc':
                        aligned_alloc_num += 1
                    elif hook_type == b'valloc':
                        valloc_num += 1
                    elif hook_type == b'pvalloc':
                        pvalloc_num += 1
                    else:
                        raise Exception('not reached')

                elif hook_type == b'free':
                    free_num += 1

                    if addr not in addr2size:
                        key_not_found += 1
                        continue

                    heap_history[heap_history_num] = heap_history[heap_history_num -
                                                                  1] - addr2size[addr]
                    del addr2size[addr]
                elif hook_type == b'realloc':
                    realloc_num += 1

                    if addr not in addr2size:
                        key_not_found += 1
                        continue

                    heap_history[heap_history_num] = heap_history[heap_history_num -
                                                                  1] - addr2size[addr]
                    del addr2size[addr]
                    heap_history[heap_history_num] += size
                    addr2size[new_addr] = size
                elif hook_type == b'malloc_usable_size':
                    malloc_usable_size_num += 1

                heap_history_num += 1

            progress_bar.finish()
            print('key_not_found =', key_not_found)
            print('malloc is called {} times'.format(malloc_num))
            print('calloc is called {} times'.format(calloc_num))
            print('realloc is called {} times'.format(realloc_num))
            print('free is called {} times'.format(free_num))
            print('posix_memalign is called {} times'.format(posix_memalign_num))
            print('memalign is called {} times'.format(memalign_num))
            print('aligned_alloc is called {} times'.format(aligned_alloc_num))
            print('valloc is called {} times'.format(valloc_num))
            print('pvalloc is called {} times'.format(pvalloc_num))
            print('malloc_usable_size is called {} times'.format(
                malloc_usable_size_num))


def visualize(output_fname):
    fig = plt.figure(figsize=(16, 16))
    ax = fig.add_subplot(1, 1, 1)

    ax.plot(heap_history)
    ax.set_xlabel('Hook index')
    ax.set_ylabel('Accumulated heap allocation size (bytes)')

    plt.savefig(output_fname)
    print('Figure is saved as {}'.format(output_fname))


if __name__ == '__main__':
    read_heap_history(sys.argv[1])

    heap_history.resize(heap_history_num)

    # Assumes 'heaplog.{pid}.log'
    input_fname = os.path.basename(sys.argv[1]).split('.')
    visualize('{}.{}.pdf'.format(input_fname[0], input_fname[1]))
