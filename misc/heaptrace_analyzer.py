#! /usr/bin/python3
import sys
import os
import matplotlib.pyplot as plt

class AllocInfo:
    def __init__(self, size, align, addr, time):
        self.size = size
        self.align = align
        self.addr = addr
        self.time = time
    
    def __str__(self):
        return f"alloc({self.size}, {self.align}) -> {hex(self.addr)} [ {self.time} ns ]"

class DeallocInfo:
    def __init__(self, addr, time):
        self.addr = addr
        self.time = time
    
    def __str__(self):
        return f"dealloc({hex(self.addr)}) [ {self.time} ns ]"

class AllocZeroedInfo:
    def __init__(self, size, addr, time):
        self.size = size
        self.addr = addr
        self.time = time
    
    def __str__(self):
        return f"alloc_zeroed({self.size}) -> {hex(self.addr)} [ {self.time} ns ]"

class ReallocInfo:
    def __init__(self, old_addr, new_size, new_addr, time):
        self.old_addr = old_addr
        self.new_size = new_size
        self.new_addr = new_addr
        self.time = time
    
    def __str__(self):
        return f"realloc({hex(self.old_addr)}, {self.new_size}) -> {hex(self.new_addr)} [ {self.time} ns ]"

class GetBlockSizeInfo:
    def __init__(self, addr, size, time):
        self.addr = addr
        self.size = size
        self.time = time

    def __str__(self):
        return f"get_block_size({hex(self.addr)}) -> {self.size} [ {self.time} ns ]"

class HeaphookAnalyzer:
    def __init__(self, input_file_name):
        self.input_file_name = input_file_name

        # list of XXXInfo (AllocInfo, DeallocInfo, ...)
        self.trace_data = []
        # list of allocated memory size transitions
        self.allocated_memory_size_history = [0]

        # counter for each method
        self.alloc_num = 0
        self.dealloc_num = 0
        self.alloc_zeroed_num = 0
        self.realloc_num = 0
        self.get_block_size_num = 0
        self.total_num = 0

        # counter for the strange phenomenon
        self.alloc_after_alloc = 0
        self.dealloc_before_alloc = 0
        self.realloc_before_alloc = 0
        self.get_block_size_before_alloc = 0
        
        with open(self.input_file_name, "r") as file:
            # allocated memory address to its size
            addr2size = dict()
            # current allocated memory size
            allocated_memory_size = 0

            for line in file:
                self.total_num += 1
                lst = list(line.split(", "))

                if lst[0] == "alloc":
                    self.alloc_num += 1
                    info = AllocInfo(
                        int(lst[1]),        # size
                        int(lst[2]),        # align
                        int(lst[3], 16),    # addr
                        int(lst[4]))        # time
                    self.trace_data.append(info)
                    if info.align > 1:
                        print(info)
                    if info.addr in addr2size: # allocating the same area twice
                        print(hex(info.addr))
                        self.alloc_after_alloc += 1
                    addr2size[info.addr] = info.size
                    allocated_memory_size += info.size

                elif lst[0] == "dealloc":
                    self.dealloc_num += 1
                    info = DeallocInfo(
                        int(lst[1], 16),    # addr
                        int(lst[2]))        # time
                    self.trace_data.append(info)
                    if info.addr not in addr2size: # try deallocate area that is not allocated
                        self.dealloc_before_alloc += 1
                    else:
                        size = addr2size.pop(info.addr)
                        allocated_memory_size -= size

                elif lst[0] == "alloc_zeroed":
                    self.alloc_zeroed_num += 1
                    info = AllocZeroedInfo(
                        int(lst[1]),        # size
                        int(lst[2], 16),    # addr
                        int(lst[3]))        # time
                    self.trace_data.append(info)
                    if info.addr in addr2size: # allocating the same area twice
                        self.alloc_after_alloc += 1
                    addr2size[info.addr] = info.size
                    allocated_memory_size += info.size

                elif lst[0] == "realloc":
                    self.realloc_num += 1
                    info = ReallocInfo(
                        int(lst[1], 16),    # old_addr
                        int(lst[2]),        # new_size
                        int(lst[3], 16),    # new_addr
                        int(lst[4]))        # time
                    self.trace_data.append(info)
                    if info.old_addr not in addr2size: # try reallocate area that is not allocated
                        self.realloc_before_alloc += 1
                    else:
                        old_size = addr2size.pop(info.old_addr)
                        addr2size[info.new_addr] = info.new_size
                        allocated_memory_size += info.new_size - old_size

                else: # get_block_size
                    self.get_block_size_num += 1
                    info = GetBlockSizeInfo(
                        int(lst[1], 16),    # addr
                        int(lst[2]),        # size
                        int(lst[3]))        # time
                    self.trace_data.append(info)
                    if info.addr not in addr2size: # try get_block_size area that is not allocated
                        self.get_block_size_before_alloc += 1

                self.allocated_memory_size_history.append(allocated_memory_size)
            
        self.alloc_info_list = list(filter(lambda info: isinstance(info, AllocInfo), self.trace_data))
        self.dealloc_info_list = list(filter(lambda info: isinstance(info, DeallocInfo), self.trace_data))
        self.alloc_zeroed_info_list = list(filter(lambda info: isinstance(info, AllocZeroedInfo), self.trace_data))
        self.realloc_info_list = list(filter(lambda info: isinstance(info, ReallocInfo), self.trace_data))
        self.get_block_size_info_list = list(filter(lambda info: isinstance(info, GetBlockSizeInfo), self.trace_data))
    
    def show_analysis_summary(self):
        print(f"alloc is called {self.alloc_num} times")
        print(f"dealloc is called {self.dealloc_num} times")
        print(f"alloc_zeroed is called {self.alloc_zeroed_num} times")
        print(f"realloc is called {self.realloc_num} times")
        print(f"get_block_size is called {self.get_block_size_num} times")
        print(f"total is {self.total_num}")
        print("")
        print(f"the number of alloc after alloc is {self.alloc_after_alloc}")
        print(f"the number of dealloc before alloc is {self.dealloc_before_alloc}")
        print(f"the number of realloc before alloc is {self.realloc_before_alloc}")
        print(f"the number of get_block_size before alloc is {self.get_block_size_before_alloc}")
    
    def plot_allocated_memory_size_history(self, output_file_name):
        x = list(range(len(self.allocated_memory_size_history)))
        y = self.allocated_memory_size_history
        plt.plot(x, y)
        plt.xlabel("Method index")
        plt.ylabel("Accumulated heap allocation size (bytes)")
        plt.savefig(output_file_name, dpi=300)
        plt.close()

    def plot_method_performance(self, info_list, output_file_name):
        x = list(range(len(info_list)))
        y = [info.time for info in info_list]
        plt.plot(x, y, 'o', markersize=1)
        if len(y) > 0:
            average = sum(y) / len(y)
            plt.axhline(y=average, linestyle='--', label=f"average = {average:.2f}", color="orange")
            plt.legend()
        plt.xlabel("Method index")
        plt.ylabel("Processing time [ns]")
        plt.savefig(output_file_name, dpi=300)
        plt.close()

def create_png_file_name(file_name, addtext):
    dirname = os.path.dirname(input_file_name)
    filename = os.path.basename(input_file_name)
    filebase = os.path.splitext(input_file_name)[0]
    return dirname + filebase + addtext + ".png"

if __name__ == '__main__':
    input_file_name = sys.argv[1]
    analyzer = HeaphookAnalyzer(input_file_name)
    analyzer.show_analysis_summary()
    analyzer.plot_allocated_memory_size_history(create_png_file_name(input_file_name, "_history_of_heap_allocation"))
    analyzer.plot_method_performance(analyzer.alloc_info_list, create_png_file_name(input_file_name, "_alloc_performance"))
    analyzer.plot_method_performance(analyzer.dealloc_info_list, create_png_file_name(input_file_name, "_dealloc_performance"))
    analyzer.plot_method_performance(analyzer.alloc_zeroed_info_list, create_png_file_name(input_file_name, "_alloc_zeroed_performance"))
    analyzer.plot_method_performance(analyzer.realloc_info_list, create_png_file_name(input_file_name, "_realloc_performance"))
    analyzer.plot_method_performance(analyzer.get_block_size_info_list, create_png_file_name(input_file_name, "_get_block_size_performance"))