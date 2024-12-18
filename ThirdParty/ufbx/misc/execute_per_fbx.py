#!/usr/bin/env python3
import argparse
import os
import time
import subprocess

parser = argparse.ArgumentParser(usage="execute_per_fbx.py --exe loader --root .")
parser.add_argument("--exe", help="Executable to run")
parser.add_argument("--root", default=".", help="Root path to search from")
parser.add_argument("--start", default="", help="Top-level file to start from")
parser.add_argument("--list", default="", help="List of files to use instead of root")
parser.add_argument("--verbose", action="store_true", help="Verbose information")
parser.add_argument("--allow-fail", action="store_true", help="Verbose information")
parser.add_argument("--cycles", default=1, type=int, help="Number of cycles to load the data")
parser.add_argument("-p", action="append", help="Run multiple permutations, use with #p")
parser.add_argument('remainder', nargs="...")
argv = parser.parse_args()

begin = time.time()

num_tested = 0
num_fail = 0
total_size = 0

def gather_files():
    if argv.list:
        with open(argv.list, "rt", encoding="utf-8") as f:
            for line in f:
                yield line.rstrip()
    else:
        for root, dirs, files in os.walk(argv.root):
            for file in files:
                path = os.path.join(root, file)
                yield path

for cycle in range(argv.cycles):
    for path in gather_files():
        file = os.path.basename(path)
        if not file.lower().endswith(".fbx"): continue
        if file.lower().endswith(".ufbx-fail.fbx"):
            num_fail += 1
            continue
        size = os.stat(path).st_size
        display = os.path.relpath(path, argv.root) if not argv.list else path

        if argv.start and display < argv.start:
            continue

        print(f"-- {display}", flush=True)
        total_size += size

        ps = [""]
        if argv.p:
            ps = argv.p

        for p in ps:
            if argv.exe:
                rest = argv.remainder[1:]

                if "#p" in rest:
                    ix = rest.index("#p")
                    rest[ix] = p.encode("utf-8")

                if "#" in rest:
                    ix = rest.index("#")
                    rest[ix] = path.encode("utf-8")
                    args = [argv.exe] + rest
                else:
                    args = [argv.exe, path.encode("utf-8")] + rest

                if argv.verbose:
                    cmdline = subprocess.list2cmdline(args)
                    print(f"$ {cmdline}")

                if argv.allow_fail:
                    try:
                        subprocess.check_call(args)
                    except Exception as e:
                        print("Failed to load")
                else:
                    subprocess.check_call(args)
        num_tested += 1

end = time.time()
dur = end - begin
print()
print("Success!")
print(f"Loaded {num_tested} files in {int(dur//60)}min {int(dur%60)}s.")
print(f"Processed {total_size/1e9:.2f}GB at {total_size/1e6/dur:.2f}MB/s.")
print(f"Ignored {num_fail} invalid files.")
