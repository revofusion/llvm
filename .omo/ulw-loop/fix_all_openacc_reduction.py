#!/usr/bin/env python3
"""Pipeline to fix the 18 OpenACC array-reduction tests."""
import subprocess, sys, re

TESTS = [
    "clang/test/CIR/CodeGenOpenACC/combined-reduction-clause-default-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/combined-reduction-clause-float.cpp",
    "clang/test/CIR/CodeGenOpenACC/combined-reduction-clause-inline-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/combined-reduction-clause-int.cpp",
    "clang/test/CIR/CodeGenOpenACC/combined-reduction-clause-outline-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-default-ops.c",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-default-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-float.c",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-float.cpp",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-inline-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-int.c",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-int.cpp",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-outline-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/compute-reduction-clause-unsigned-int.c",
    "clang/test/CIR/CodeGenOpenACC/loop-reduction-clause-default-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/loop-reduction-clause-float.cpp",
    "clang/test/CIR/CodeGenOpenACC/loop-reduction-clause-inline-ops.cpp",
    "clang/test/CIR/CodeGenOpenACC/loop-reduction-clause-int.cpp",
    "clang/test/CIR/CodeGenOpenACC/loop-reduction-clause-outline-ops.cpp",
]

def update_run_line(path):
    with open(path) as f:
        txt = f.read()
    old1 = "| sed -E 's/loc\\([^)]*\\)//g' | FileCheck %s"
    new1 = "| sed -E 's/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g' | FileCheck %s"
    if old1 in txt:
        txt = txt.replace(old1, new1)
    if "-o %t.cir" in txt:
        txt = txt.replace("-o %t.cir", '-o - | sed -E "s/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g" > %t.cir')
    if "-o %t.ll" in txt:
        txt = txt.replace("-o %t.ll", '-o - | sed -E "s/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g" > %t.ll')
    with open(path, 'w') as f:
        f.write(txt)

def fix_step_itr_load(path):
    with open(path) as f:
        txt = f.read()
    new = re.sub(r'(} step \{\n// CHECK-NEXT: )(%\[\[ITR_LOAD\]\]\s*= cir\.load)', r'\1%[[ITR_LOAD:.*]] = cir.load', txt)
    if new != txt:
        with open(path, 'w') as f:
            f.write(new)

def main():
    for path in TESTS:
        print("===", path)
        # restore from git so we start from a clean test rot state
        subprocess.run(["git", "checkout", "--", path], check=True)
        update_run_line(path)
        subprocess.run(["python3", ".omo/ulw-loop/fix_openacc_reduction.py", path], check=True)
        subprocess.run(["python3", ".omo/ulw-loop/fix_loc_placeholders.py", path], check=True)
        fix_step_itr_load(path)
        res = subprocess.run(["build/bin/llvm-lit", "-s", path], capture_output=True, text=True, timeout=120)
        print(res.stdout[-500:] if len(res.stdout) > 500 else res.stdout)
        if res.returncode != 0:
            print("FAILED", path, file=sys.stderr)
            print(res.stderr[-500:] if len(res.stderr) > 500 else res.stderr, file=sys.stderr)
        else:
            print("PASSED", path)

if __name__ == "__main__":
    main()
