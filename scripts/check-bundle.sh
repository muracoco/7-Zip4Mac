#!/bin/bash
set -euo pipefail
APP=${1:?app path required}
python3 - "$APP" <<'PY'
import pathlib, subprocess, sys, re
root = pathlib.Path(sys.argv[1]); count=0
for f in root.rglob('*'):
    if not f.is_file() or f.is_symlink(): continue
    if 'Mach-O' not in subprocess.check_output(['file','-b',str(f)],text=True): continue
    count+=1
    output=subprocess.check_output(['otool','-L',str(f)],text=True)
    id_output=subprocess.run(['otool','-D',str(f)],capture_output=True,text=True).stdout.splitlines()[1:]
    for line in output.splitlines()[1:]:
        if not line.startswith('\t'): continue  # universal binary architecture headings
        dep=line.strip().split(' (')[0]
        if dep in id_output: continue  # LC_ID_DYLIB is not a loaded dependency.
        if dep.startswith('/') and not dep.startswith(('/System/Library/','/usr/lib/')):
            raise SystemExit(f'External runtime dependency: {f}: {dep}')
    load_commands=subprocess.check_output(['otool','-l',str(f)],text=True)
    for rpath in re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset',load_commands):
        if rpath.startswith('/') and not rpath.startswith(('/System/Library/','/usr/lib/')):
            raise SystemExit(f'External runtime rpath: {f}: {rpath}')
print(f'Bundle dependency check passed: {count} Mach-O files; system/@rpath dependencies only')
PY
codesign --verify --deep --strict "$APP"
