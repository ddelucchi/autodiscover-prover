import os

base = r"c:\Users\deluc\Desktop\Autodiscoverer\attempt2.1\AutoDiscoverProver\src\domain"
f = os.path.join(base, "CayleyLambda.hpp")

with open(f, 'r', encoding='utf-8') as fh:
    lines = fh.readlines()

print(f"Original: {len(lines)} lines")
print(f"First line: {repr(lines[0][:40])}")

# Find end of block comment
end_idx = -1
for i, line in enumerate(lines):
    if line.strip() == '*/':
        end_idx = i
        break

print(f"Block comment ends at line {end_idx + 1} (0-indexed: {end_idx})")

new_header = [
    '/**\n',
    ' * @file CayleyLambda.hpp\n',
    ' * @brief Cayley-Lambda Map, Hopf Manifold, Torus Geometry, and Scale-Covariant Spaces\n',
    ' * See calculusnumberunification.txt for full mathematical details.\n',
    ' */\n',
]

new_lines = new_header + lines[end_idx + 1:]
print(f"New: {len(new_lines)} lines")

# Also fix inline comment with Unicode
for i, line in enumerate(new_lines):
    if '\u2102*/' in line:
        old_line = line
        new_lines[i] = '        // C_star/<z~Lz> = T^2  encoded as:\n'
        print(f"Fixed inline comment at new line {i + 1}")

with open(f, 'w', encoding='utf-8', newline='\r\n') as fh:
    fh.writelines(new_lines)

print(f"Written {len(new_lines)} lines to {f}")
print("SUCCESS")
