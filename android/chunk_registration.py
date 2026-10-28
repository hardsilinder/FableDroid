#!/usr/bin/env python3
"""Bound compiler work for the one-time function registration routine."""
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
source = path.read_text()
marker = '// Android registration chunks; guest registration order is unchanged.'
if marker in source:
    print('Registration already chunked')
    raise SystemExit(0)
match = re.fullmatch(r'(.*?)void fable_2_RegisterFunctions\(rex::runtime::IModuleRegistrar\* registrar\) \{(.*?)\}\s*', source, re.S)
if not match:
    raise SystemExit('Unexpected registration routine format')
lines = [line for line in match[2].splitlines() if line.strip()]
if not lines or any(not re.fullmatch(r'  registrar->SetFunction\(0x[0-9A-Fa-f]+, [A-Za-z_0-9]+\);', line) for line in lines):
    raise SystemExit('Unexpected statement in registration routine')
parts = [match[1], marker + '\n']
names = []
for start in range(0, len(lines), 256):
    name = f'fable2_RegisterChunk_{len(names)}'
    names.append(name)
    parts.append(f'static __attribute__((noinline)) void {name}(rex::runtime::IModuleRegistrar* registrar) {{\n')
    parts.extend(line + '\n' for line in lines[start:start + 256])
    parts.append('}\n\n')
parts.append('void fable_2_RegisterFunctions(rex::runtime::IModuleRegistrar* registrar) {\n')
parts.extend(f'  {name}(registrar);\n' for name in names)
parts.append('}\n')
output = ''.join(parts)
# Ensure this transformation preserves every registration and its order.
assert re.findall(r'registrar->SetFunction\([^\n]+', source) == re.findall(r'registrar->SetFunction\([^\n]+', output)
path.write_text(output)
print(f'Preserved {len(lines)} registrations across {len(names)} bounded startup routines')
