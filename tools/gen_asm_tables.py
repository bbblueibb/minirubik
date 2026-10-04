import re
from pathlib import Path

src = Path("tables.h").read_text()

arrays = [
    ("permutation_transition", "half"),
    ("orientation_transition", "half"),
    ("permutation_pdb", "byte"),
    ("orientation_pdb", "byte"),
    ("corner4_pdb", "byte"),
]

out = []

out.append('.section .rodata')
out.append('.align 2')
out.append('')

for name, directive in arrays:
    pattern = rf'static const\s+\w+\s+{name}\s*\[[^\]]+\]\s*=\s*\{{(.*?)\}};'
    m = re.search(pattern, src, re.S)

    if not m:
        raise SystemExit(f"could not find {name}")

    values = re.findall(r'\d+', m.group(1))

    out.append(f'.globl {name}')
    out.append(f'{name}:')

    per_line = 12 if directive == "byte" else 8

    for i in range(0, len(values), per_line):
        chunk = ", ".join(values[i:i + per_line])
        out.append(f'    .{directive} {chunk}')

    out.append('')

Path("tables_rv32i.S").write_text("\n".join(out) + "\n")

print("generated tables_rv32i.S")
