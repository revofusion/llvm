#!/usr/bin/env python3
import sys, re

def fix(text):
    # Remove loc placeholders after CIR types and before ) or ,
    text = re.sub(r'(!(?:cir|acc)\.[^{%,]+?)\s*\{\{\.\*\}\}([),])', r'\1\2', text)
    # Some test files had an extra ) inside the old loc placeholder for block-arg lines.
    lines = []
    for line in text.splitlines():
        if '^bb0' in line:
            line = line.replace(')):', '):').replace(')),', '),')
            if line.endswith('))'):
                line = line[:-2] + '):'
        lines.append(line)
    return '\n'.join(lines)

for path in sys.argv[1:]:
    with open(path) as f:
        txt = f.read()
    new = fix(txt)
    if new != txt:
        with open(path, 'w') as f:
            f.write(new)
        print('fixed', path)
    else:
        print('no changes', path)
