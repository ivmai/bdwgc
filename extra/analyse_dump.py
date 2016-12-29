#!/usr/bin/env python3

# Copyright (C) 2016 Paul Bone
#
# THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
# OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
#
# Permission is hereby granted to use or copy this program
# for any purpose,  provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is granted,
# provided the above notices are retained, and a notice that the code was
# modified is included with the above copyright notice.

#
# Analyse GC_dump() information and provide various tables and graphs
# (written to the current working directory) showing heap utilisation.
# See http://paul.bone.id.au/2016/12/29/more-about-memory-fragmentation/ for
# an example.
#
# This script was written for my own purposes, to adapt it to your situation
# you will need to modify the code.  A good place to start is probably the
# analyse function.
#

import csv
import re
import sys
import math
import matplotlib as mpl
import matplotlib.pyplot as plt

hblk_size = 4096

sky_blue = "#4da2cc"
trees_shadow = "#005374"

converter = mpl.colors.ColorConverter()
colors =      ["#4da2cc", "#b74dcc", "#cc774d", "#62cc4d", "#FF0000"]
colors_dark = [tuple(co*0.75 for co in converter.to_rgb(c)) for c in colors]

re_begin_dump = re.compile("^\*\*\*GC Dump (.*)$")
re_time = re.compile("^Time: (.*)$")
re_blocks_in_use = re.compile("^\*\*\*Blocks in use:")
re_blocks_end = re.compile("^blocks= ")
re_num = re.compile("\d+")

def round_to(num, limit):
    return (int(math.floor(num / limit)) + 1) * limit

def safe_fdiv(n, d):
    try:
        return float(n) / float(d)
    except ZeroDivisionError:
        return 0.0

def hu_bytes(b):
    if b < 64*1024:
        return "{:,}B".format(b)
    elif b < (64*1024*1024):
        return "{:,}KB".format(round(b / 1024))
    else:
        return "{:,}MB".format(round(b / (1024*1024)))

def b_to_mb(b):
    return float(b)/(1024.0*1024.0)

class Block:
    def __init__(self, kind, size, marks):
        self.kind = int(kind)
        self.size = int(size)
        self.marks = int(re_num.match(marks).group(0))

    def used_bytes(self):
        if self.size < 4096:
            return self.marks * self.size
        else:
            return self.size

    def block_size(self):
        return round_to(self.size, 4096)

class Dump:
    def __init__(self, name):
        self.name = name
        self.time = None
        self.blocks = None

    def __repr__(self):
        return "Dump {} at {:,}".format(self.name, self.time)
   
    def __str__(self):
        total_size = sum((b.block_size() for b in self.blocks))
        used = sum((b.used_bytes() for b in self.blocks))
        return self.__repr__() + " %s/%s used." % (hu_bytes(used), hu_bytes(total_size))

    def set_time(self, time):
        self.time = time

    def set_csv(self, csv):
        csv = list(csv)

        # Drop header
        csv = csv[1:]

        # Convert everything to blocks
        self.blocks = [Block(r[0], r[1], r[2]) for r in csv]

    def kind_summary(self):
        atomic_blocks = 0
        atomic_bytes = 0
        atomic_allocations = 0
        normal_blocks = 0
        normal_bytes = 0
        normal_allocations = 0
        uncollectable_blocks = 0
        uncollectable_bytes = 0
        uncollectable_allocations = 0
        heap_size = 0

        for b in self.blocks:
            bytes_ = b.size * b.marks
            allocations = b.marks
            heap_size += b.block_size()
            if b.kind == 0:
                atomic_blocks += 1
                atomic_bytes += bytes_
                atomic_allocations += allocations
            elif b.kind == 1:
                normal_blocks += 1
                normal_bytes += bytes_
                normal_allocations += allocations
            elif b.kind == 2:
                uncollectable_blocks += 1
                uncollectable_bytes += bytes_
                uncollectable_allocations += allocations
            else:
                print("Warning un1known block kind: " + b.kind)

        return (self.time,
                atomic_blocks, atomic_bytes, atomic_allocations,
                normal_blocks, normal_bytes, normal_allocations,
                uncollectable_blocks, uncollectable_bytes,
                uncollectable_allocations, heap_size, self.name)

    def block_size_summary(self):
        sizes = dict()
        heap_size = 0
        for b in self.blocks:
            size = b.size
            heap_size += b.block_size()
            try:
                sizes[size].count += 1
                sizes[size].marks += b.marks
            except KeyError:
                sizes[size] = BlockSizeInfo(b.marks, b.block_size())

        return (self.time, self.name, heap_size, sizes)

class BlockSizeInfo:
    def __init__(self, marks, block_size):
        self.count = 1
        self.marks = marks
        self.block_size = block_size

def read_csv_rows(f):
    for line in f:
        if re_blocks_end.match(line):
            raise StopIteration
        else:
            yield line
    raise StopIteration

def read(filename):
    data = []
    dump = None
    f = open(filename)

    for line in f:
        r = re_begin_dump.match(line)
        if r:
            if dump:
                data += [dump]
            name = r.group(1)
            if name.startswith("collection"):
                name = None
            dump = Dump(name)
            continue
        r = re_time.match(line)
        if r:
            dump.set_time(int(r.group(1)))
            continue
        r = re_blocks_in_use.match(line)
        if r:
            dump.set_csv(csv.reader(read_csv_rows(f)))
            continue

    f.close()

    if dump:
        data += [dump]
    return data

def analyse(data):
    def make_kinds_over_time_table(rows):
        print("Kinds over time")

        header_fmt = "{:^8} {:^6} {:^7} {:^13}"
        row_fmt = "{:>6,}ms {:>6,} {:>7,} {:>13,}"

        print(header_fmt.format("Time", "Atomic", "Normal", "Uncollectable"))

        for r in rows:
            time = round(r[0] / 1000)
            atomic = r[1]
            normal = r[4]
            uncollectable = r[7]
            print(row_fmt.format(time, atomic, normal, uncollectable))

        print()

    def make_utilisation_over_time_table(rows):
        print("Utilisation over time")

        header_fmt = "{:^8} {:^7} {:^10} {:^7} {:^9} {:^11} {:^4}"
        row_fmt = "{:>6,}ms {:>7,} {:>10,} {:>7} {:>9} {:>11,.1f} {:>10.1f}%"

        print(header_fmt.format(
            "Time", "Blocks", "Allocs", "Bytes", "Heap size", "Bytes/Alloc",
            "Utilisation"))
        for r in rows:
            time = round(r[0] / 1000)
            blocks = r[1] + r[4] + r[7]
            bytes_ = r[2] + r[5] + r[8]
            allocs = r[3] + r[6] + r[9]
            bytes_alloc = safe_fdiv(bytes_, allocs)
            heap_size = r[10]
            heap_utilisation = safe_fdiv(bytes_, heap_size)
            if r[11]:
                print(r[11])
            print(row_fmt.format(time, blocks, allocs, hu_bytes(bytes_),
                hu_bytes(heap_size), bytes_alloc, heap_utilisation*100.0))

        print()

    def make_utilisation_over_time_plot(rows):
        rows1 = [(round(r[0] / 1000),
                  b_to_mb(r[2] + r[5] + r[8]),
                  b_to_mb(r[10]))
                 for r in rows]

        h_tot = plt.fill_between([r[0] for r in rows1],
                [r[2] for r in rows1], 0, color=colors[0])
        h_used = plt.fill_between([r[0] for r in rows1],
                [r[1] for r in rows1], 0, color=colors[2])
        plt.legend([h_tot, h_used], ["Total", "Used"], loc="upper left")
        plt.grid(True)
        plt.xlabel("Time (ms)")
        plt.ylabel("Size (MB)")
        plt.title("Heap Usage")
        plt.savefig("utilisation.svg", format="svg")
        plt.close()

    def make_size_utilisation_table(rows):
        print("Block sizes over over time")

        header1_fmt = "{:^8} {:^10}"
        header2_fmt = (" "*8) + " {:^10} {:^10} {:^10} {:^7} {:^10} {:^6}"
        row1_fmt = "{:>6,}ms {:>10}"
        row2_fmt = (" "*8) + " {:>10} {:>10,} {:>10} {:>7,} {:>10} {:>5.1f}%"

        print(header1_fmt.format("Time", "Heap size"))
        print(header2_fmt.format("Alloc size", "Allocs", "Used bytes",
            "Blocks", "Total size", "Util"))

        for r in rows:
            if r[1]:
                print(r[1])

            time = round(r[0] / 1000)
            heap_size = r[2]
            print(row1_fmt.format(time, hu_bytes(heap_size)))

            for s in sorted(r[3].keys()):
                block_info = r[3][s]
                alloc_size = s*block_info.marks
                total_size = block_info.block_size * block_info.count
                util = float(alloc_size) / float(total_size)
                print(row2_fmt.format(hu_bytes(s),
                    block_info.marks, hu_bytes(alloc_size),
                    block_info.count, hu_bytes(total_size),
                    util*100.0))

        print()

    def make_size_utilisation_plot(rows):
        size_other = []
        size_64 = []
        size_48 = []
        size_32 = []
        size_16 = []
        times = [round(r[0] / 1000) for r in rows]

        for r in rows:
            size_other_alloc = 0
            size_other_total = 0
            size_64_total = 0
            size_48_total = 0
            size_32_total = 0
            size_16_total = 0
            for s in r[3].keys():
                block_info = r[3][s]
                # alloc_size = s*block_info.marks
                total_size = block_info.block_size * block_info.count
                if s > 64:
                    size_other_total += total_size
                if s == 64:
                    size_64_total += total_size
                if s == 48:
                    size_48_total += total_size
                if s == 32:
                    size_32_total += total_size
                if s == 16:
                    size_16_total += total_size

            size_other += [b_to_mb(size_other_total)]
            accum = size_other_total + size_64_total
            size_64 += [b_to_mb(accum)]
            accum += size_48_total
            size_48 += [b_to_mb(accum)]
            accum += size_32_total
            size_32 += [b_to_mb(accum)]
            accum += size_16_total
            size_16 += [b_to_mb(accum)]
    
        p_other = plt.fill_between(times, size_other, 0, color="#005374")
        p_64 = plt.fill_between(times, size_64, size_other, color=colors[2])
        p_48 = plt.fill_between(times, size_48, size_64, color=colors[3])
        p_32 = plt.fill_between(times, size_32, size_48, color=colors[1])
        p_16 = plt.fill_between(times, size_16, size_32, color=colors[0])
        plt.legend([p_16, p_32, p_48, p_64, p_other],
                ["16", "32", "48", "64", "Other"], loc="upper left")
        plt.grid(True)
        plt.xlabel("Time (ms)")
        plt.ylabel("Heap Usage (MB)")
        plt.title("Heap usage by object size")
        plt.savefig("size-classes.svg", format="svg")
        plt.close()

    def make_size_utilisation_plot2(rows):
        size_other = []
        size_64 = []
        size_48_allocl = []
        size_48 = []
        size_32 = []
        size_16_allocl = []
        size_16 = []
        times = [round(r[0] / 1000) for r in rows]

        for r in rows:
            size_other_alloc = 0
            size_other_total = 0
            size_64_total = 0
            size_48_alloc = 0
            size_48_total = 0
            size_32_total = 0
            size_16_alloc = 0
            size_16_total = 0
            for s in r[3].keys():
                block_info = r[3][s]
                alloc_size = s*block_info.marks
                total_size = block_info.block_size * block_info.count
                if s > 64:
                    size_other_total += total_size
                if s == 64:
                    size_64_total += total_size
                if s == 48:
                    size_48_alloc += alloc_size
                    size_48_total += total_size
                if s == 32:
                    size_32_total += total_size
                if s == 16:
                    size_16_alloc += alloc_size
                    size_16_total += total_size

            size_other += [b_to_mb(size_other_total)]
            accum = size_other_total + size_64_total
            size_64 += [b_to_mb(accum)]
            size_48_allocl += [b_to_mb(accum + size_48_alloc)]
            accum += size_48_total
            size_48 += [b_to_mb(accum)]
            accum += size_32_total
            size_32 += [b_to_mb(accum)]
            size_16_allocl += [b_to_mb(accum + size_16_alloc)]
            accum += size_16_total
            size_16 += [b_to_mb(accum)]

        p_other = plt.fill_between(times, size_other, 0, color="#005374")
        p_64 = plt.fill_between(times, size_64, size_other, color=colors[2])
        p_48_alloc = plt.fill_between(times, size_48_allocl, size_64,
                color=colors_dark[3])
        p_48 = plt.fill_between(times, size_48, size_48_allocl, color=colors[3])
        p_32 = plt.fill_between(times, size_32, size_48, color=colors[1])
        p_16_alloc = plt.fill_between(times, size_16_allocl, size_32,
                color=colors_dark[0])
        p_16 = plt.fill_between(times, size_16, size_16_allocl, color=colors[0])
        plt.legend([p_16, p_16_alloc, p_48, p_48_alloc],
                ["16 free", "16 used", "48 free", "48 used",], loc="upper left")
        plt.grid(True)
        plt.xlabel("Time (ms)")
        plt.ylabel("Heap Usage (MB)")
        plt.title("Heap usage by object size")
        plt.savefig("size-classes2.svg", format="svg")
        plt.close()
    
    def make_size_utilisation_obj_32_plot(rows):
        allocl = []
        totall = []
        times = [round(r[0] / 1000) for r in rows]

        for r in rows:
            size_32_alloc = 0
            size_32_total = 0
            for s in r[3].keys():
                block_info = r[3][s]
                alloc_size = s*block_info.marks
                total_size = block_info.block_size * block_info.count
                if s == 32:
                    size_32_alloc += alloc_size
                    size_32_total += total_size

            allocl += [b_to_mb(size_32_alloc)]
            totall += [b_to_mb(size_32_total)]

        p_alloc = plt.fill_between(times, allocl, 0, color=colors_dark[1])
        p_total = plt.fill_between(times, totall, allocl, color=colors[1])
        plt.legend([p_total, p_alloc], ["Free", "Used"], loc="upper left")
        plt.grid(True)
        plt.xlabel("Time (ms)")
        plt.ylabel("Heap usage (MB)")
        plt.title("Heap usage for 32 byte objects")
        plt.savefig("usage-32.svg", format="svg")
        plt.close()

    kinds_over_time = [d.kind_summary() for d in data]
    make_kinds_over_time_table(kinds_over_time)

    make_utilisation_over_time_table(kinds_over_time)
    make_utilisation_over_time_plot(kinds_over_time)

    size_over_time = [d.block_size_summary() for d in data]
    make_size_utilisation_table(size_over_time)
    make_size_utilisation_plot(size_over_time)
    make_size_utilisation_plot2(size_over_time)
    make_size_utilisation_obj_32_plot(size_over_time)

def main():
    data = read(sys.argv[1])
    analyse(data)

if __name__ == "__main__":
    main()

