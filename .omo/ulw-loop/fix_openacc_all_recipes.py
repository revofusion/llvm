#!/usr/bin/env python3
"""Regenerate OpenACC reduction recipe CHECK blocks from current CIR output.

This replaces the entire recipe (init + combiner) for every
acc.reduction.recipe in the test file, leaving surrounding CHECKs and source
untouched.  It is intentionally conservative: it only edits recipe blocks and
the first RUN line.
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
    """Return dict name -> list of raw lines for the full recipe block."""
    lines = cir.splitlines()
    recipes = {}
    i = 0
    while i < len(lines):
        m = re.match(r'^(\s*)acc\.reduction\.recipe\s+@(\S+)\s+.*\sinit\s*\{\s*$', lines[i])
        if not m:
            i += 1
            continue
        name = m.group(2)
        start = i
        depth = 1
        i += 1
        while i < len(lines) and depth > 0:
            # Count real braces only (ignore braces inside string literals is fine
            # here because CIR strings are simple identifiers).
            depth += lines[i].count('{')
            depth -= lines[i].count('}')
            i += 1
        recipes[name] = lines[start:i]
    return recipes


def anon_recipe(raw_lines):
    """Turn a raw recipe block into FileCheck-ready CHECK lines."""
    if not raw_lines:
        return []

    out = []
    # First line is the recipe header; keep it exactly.
    header = raw_lines[0].strip()
    out.append(header)

    # Find init arg name (first ^bb0 in the block).
    init_arg = None
    combiner_lhs = None
    combiner_rhs = None

    for line in raw_lines:
        if re.match(r'^\^bb\d+\(%arg\d+:', line):
            args = re.findall(r'%(arg\d+)', line)
            if init_arg is None:
                if args:
                    init_arg = args[0]
            else:
                # combiner block
                if len(args) >= 2:
                    combiner_lhs, combiner_rhs = args[0], args[1]
                elif len(args) == 1:
                    combiner_lhs = args[0]

    def repl(line):
        # Normalize block labels.
        line = re.sub(r'\^bb\d+', '^bb0', line)
        # Replace the specific combiner args first so they keep named captures.
        if combiner_lhs:
            line = re.sub(r'%' + re.escape(combiner_lhs) + r'(?![0-9])', r'%[[LHSARG]]', line)
        if combiner_rhs:
            line = re.sub(r'%' + re.escape(combiner_rhs) + r'(?![0-9])', r'%[[RHSARG]]', line)
        # Replace init arg.
        if init_arg:
            line = re.sub(r'%' + re.escape(init_arg) + r'(?![0-9])', r'%[[ARG]]', line)
        # Any remaining block args become anonymous.
        line = re.sub(r'%arg\d+(?![0-9])', r'%{{.*}}', line)
        # Local SSA values.
        line = re.sub(r'%(\d+)', r'%{{.*}}', line)
        return line

    # Body lines: skip header, apply replacements.
    for line in raw_lines[1:]:
        out.append(repl(line.rstrip()))
    return out


def replace_in_test(path, recipes):
    with open(path) as f:
        lines = f.readlines()

    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        header_m = re.match(
            r'^(\s*//\s*CHECK(?:-NEXT)?:\s*acc\.reduction\.recipe\s+@(\S+)\s+.*\sinit\s*\{\s*)$',
            line)
        if header_m:
            name = header_m.group(2)
            if name in recipes:
                # Find the matching closing `}` line using brace depth, ignoring
                # FileCheck regex regions {{...}}.
                start_idx = i
                depth = 1
                j = i + 1
                while j < len(lines):
                    content = re.sub(r'\{\{.*?\}\}', '', lines[j])
                    # Count braces only in the CHECK directive portion.
                    check_m = re.match(r'\s*//\s*CHECK(?:-NEXT)?:\s*(.*)', content)
                    if check_m:
                        depth += check_m.group(1).count('{')
                        depth -= check_m.group(1).count('}')
                    if depth == 0:
                        break
                    j += 1
                end_idx = j

                # Emit the new recipe block.
                new_lines.append('// CHECK: ' + anon_recipe(recipes[name])[0].lstrip() + '\n')
                for body_line in anon_recipe(recipes[name])[1:]:
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
    if '-o %t.cir' in txt:
        txt = txt.replace('-o %t.cir', '-o - | sed -E "s/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g" > %t.cir')
    if '-o %t.ll' in txt:
        txt = txt.replace('-o %t.ll', '-o - | sed -E "s/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g" > %t.ll')
    # Tests that pipe CIR straight into FileCheck need loc stripping too.
    if '-o - | FileCheck %s' in txt and 'sed' not in txt.split('\n')[0]:
        txt = txt.replace('-o - | FileCheck %s',
                          '-o - | sed -E "s/ loc\\([^)]*\\)//g; s/ ,/,/g; s/ \\)/)/g" | FileCheck %s')
    with open(path, 'w') as f:
        f.write(txt)


def fix_step_itr_load(path):
    with open(path) as f:
        txt = f.read()
    new = re.sub(r'(}\s*step\s*\{\n//\s*CHECK(?:-NEXT)?:\s*)(%\[\[ITR_LOAD\]\]\s*=\s*cir\.load)',
                 r'\1%[[ITR_LOAD:.*]] = cir.load', txt)
    if new != txt:
        with open(path, 'w') as f:
            f.write(new)


def main():
    for path in sys.argv[1:]:
        print('processing', path)
        update_run_line(path)
        cir = emit_cir(path)
        recipes = parse_recipes(cir)
        print('  found', len(recipes), 'recipes')
        replace_in_test(path, recipes)
        fix_step_itr_load(path)
        print('fixed', path)


if __name__ == '__main__':
    main()
