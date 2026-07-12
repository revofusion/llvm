#!/usr/bin/env python3
import subprocess, sys

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

for path in TESTS:
    print('===', path)
    subprocess.run(["git", "checkout", "--", path], check=True)

subprocess.run(["python3", ".omo/ulw-loop/fix_openacc_all_recipes.py"] + TESTS, check=True)

failed = []
for path in TESTS:
    res = subprocess.run(["build/bin/llvm-lit", "-s", path], capture_output=True, text=True, timeout=120)
    out = res.stdout[-800:] if len(res.stdout) > 800 else res.stdout
    print(out)
    if res.returncode != 0:
        failed.append(path)
        err = res.stderr[-800:] if len(res.stderr) > 800 else res.stderr
        print('FAILED', path, file=sys.stderr)
        print(err, file=sys.stderr)
    else:
        print('PASSED', path)

print('\nFAILURES:', len(failed))
for f in failed:
    print(' ', f)
sys.exit(1 if failed else 0)
