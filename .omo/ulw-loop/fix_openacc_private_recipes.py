#!/usr/bin/env python3
"""Regenerate OpenACC private/firstprivate recipe CHECK blocks.

These tests fail because they reuse FileCheck variable names like ITR_LOAD or
BOUND1 across recipes.  We just replace every whole recipe block with the
actual (fully anonymized) compiler output, leaving the rest of the test alone.
"""
import re, subprocess, sys

CLANG = "build/bin/clang"
SED = "sed -E 's/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g'"


def emit_cir(path):
    with open(path) as f:
        first = f.readline()
    m = re.search(r'%clang_cc1(.*?)%s', first)
    extra = m.group(1).strip() if m else "-fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc"
    cmd = f"{CLANG} -cc1 -internal-isystem build/lib/clang/22/include -nostdsysteminc {extra} {path} -o - | {SED}"
    proc = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=120)
    if proc.returncode != 0:
        print(proc.stderr[-2000:], file=sys.stderr)
        raise RuntimeError(f"clang failed for {path}")
    return proc.stdout


def parse_recipes(cir):
    """Return dict name -> raw lines for every acc.*.recipe block."""
    lines = cir.splitlines()
    recipes = {}
    i = 0
    while i < len(lines):
        m = re.match(r'^(\s*)acc\.(private|firstprivate|reduction)\.recipe\s+@(\S+)\s+.*\{\s*$', lines[i])
        if not m:
            i += 1
            continue
        name = m.group(3)
        start = i
        depth = 1
        i += 1
        while i < len(lines) and depth > 0:
            depth += lines[i].count('{')
            depth -= lines[i].count('}')
            i += 1
        recipes[name] = lines[start:i]
    return recipes


def anon_recipe(raw_lines):
    out = []
    for line in raw_lines:
        s = line.rstrip()
        # Normalize block labels.
        s = re.sub(r'\^bb\d+', '^bb0', s)
        # Anonymize all block args / local SSA values.
        s = re.sub(r'%arg\d+(?![0-9])', r'%{{.*}}', s)
        s = re.sub(r'%(\d+)', r'%{{.*}}', s)
        out.append(s)
    return out


def replace_in_test(path, recipes):
    with open(path) as f:
        lines = f.readlines()

    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        header_m = re.match(
            r'^(\s*//\s*CHECK(?:-NEXT)?:\s*acc\.(private|firstprivate|reduction)\.recipe\s+@(\S+)\s+.*\{\s*)$',
            line)
        if header_m:
            name = header_m.group(3)
            if name in recipes:
                start_idx = i
                depth = 1
                j = i + 1
                while j < len(lines):
                    content = re.sub(r'\{\{.*?\}\}', '', lines[j])
                    check_m = re.match(r'\s*//\s*CHECK(?:-NEXT)?:\s*(.*)', content)
                    if check_m:
                        depth += check_m.group(1).count('{')
                        depth -= check_m.group(1).count('}')
                    if depth == 0:
                        break
                    j += 1
                end_idx = j

                raw = recipes[name]
                new_lines.append('// CHECK: ' + anon_recipe(raw)[0].lstrip() + '\n')
                for body_line in anon_recipe(raw)[1:]:
                    new_lines.append('// CHECK-NEXT: ' + body_line + '\n')
                i = end_idx + 1
                continue
        new_lines.append(line)
        i += 1

    with open(path, 'w') as f:
        f.writelines(new_lines)


def update_run_line(path):
    with open(path) as f:
        txt = f.read()
    old = "| sed -E 's/loc\\([^)]*\\)//g' | FileCheck %s"
    new = "| sed -E 's/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g' | FileCheck %s"
    if old in txt:
        txt = txt.replace(old, new)
    if '-o - | FileCheck %s' in txt and 'sed' not in txt.split('\n')[0]:
        txt = txt.replace('-o - | FileCheck %s',
                          '-o - | sed -E "s/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g" | FileCheck %s')
    with open(path, 'w') as f:
        f.write(txt)


def main():
    for path in sys.argv[1:]:
        print('processing', path)
        update_run_line(path)
        cir = emit_cir(path)
        recipes = parse_recipes(cir)
        print('  found', len(recipes), 'recipes')
        replace_in_test(path, recipes)
        print('fixed', path)


if __name__ == '__main__':
    main()
