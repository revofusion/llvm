#!/usr/bin/env python3
"""Update OpenACC array-reduction init CHECK blocks to match current CIR output."""
import re, subprocess, sys

CLANG = "build/bin/clang"
SED = "sed -E 's/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g'"

def emit_cir(path):
    with open(path) as f:
        first = f.readline()
    m = re.search(r'%clang_cc1(.*?)%s', first)
    extra = m.group(1).strip() if m else "-fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc"
    cmd = f"{CLANG} -cc1 -internal-isystem build/lib/clang/22/include -nostdsysteminc {extra} {path} -o - | {SED}"
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=120).stdout

def anon(line):
    # Replace SSA value references with anonymous patterns.
    line = re.sub(r'%arg\d+', r'%[[ARG:.*]]', line)
    line = re.sub(r'%\d+', r'%{{.*}}', line)
    return line

def parse_array_recipes(cir):
    recipes = []
    lines = cir.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.search(r'acc\.reduction\.recipe @reduction_(\w+)__(\S+)\s*:\s*!cir\.ptr<(!cir\.array<[^>]+>)>\s+reduction_operator\s+<(\w+)>\s+init\s*\{', line)
        if not m:
            i += 1
            continue
        op, suffix, arrtype, rop = m.groups()
        # collect init block until line that is just "  }" followed by " combiner {" on next line
        body = []
        i += 1
        while i < len(lines):
            l = lines[i]
            if 'combiner' in l and '}' in l:
                break
            body.append(l.strip())
            i += 1
        recipes.append((op, suffix, arrtype, rop, body))
    return recipes

def generate_init_check(body):
    out = []
    for l in body:
        if not l:
            continue
        l = anon(l)
        out.append("// CHECK-NEXT: " + l)
    return out

def replace_in_test(path, recipes):
    with open(path) as f:
        text = f.read()
    lines = text.splitlines(keepends=True)
    for op, suffix, arrtype, rop, body in recipes:
        decl = f"acc.reduction.recipe @reduction_{op}__{suffix}"
        # find declaration line index
        start = None
        for idx, line in enumerate(lines):
            if decl in line and "init {" in line:
                start = idx
                break
        if start is None:
            print(f"  warning: could not find {decl}", file=sys.stderr)
            continue
        # find combiner line after start
        end = None
        for j in range(start + 1, len(lines)):
            if "} combiner {" in lines[j]:
                end = j
                break
        if end is None:
            print(f"  warning: could not find combiner for {decl}", file=sys.stderr)
            continue
        # keep declaration line and combiner line, replace middle
        new_body = generate_init_check(body)
        # ensure blank CHECK-NEXT before combiner if original had one
        new_lines = [lines[start]] + [ln + "\n" for ln in new_body] + [lines[end]]
        lines = lines[:start + 1] + [ln + "\n" for ln in new_body] + lines[end:]
    with open(path, "w") as f:
        f.writelines(lines)

def main(paths):
    for path in paths:
        print("processing", path)
        cir = emit_cir(path)
        recipes = parse_array_recipes(cir)
        print(f"  found {len(recipes)} array recipes")
        replace_in_test(path, recipes)

if __name__ == "__main__":
    main(sys.argv[1:])
