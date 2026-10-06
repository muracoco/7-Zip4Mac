#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
NO_FOCUS=0
RESULT=0
CTEST_ARGS=()
if [[ -n ${PORT_TEST_REGEX:-} ]]; then CTEST_ARGS=(-R "$PORT_TEST_REGEX"); fi
if [[ ${1:-} == --no-focus ]]; then NO_FOCUS=1; shift; fi
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
FIXTURES=$(mktemp -d "${TMPDIR:-/tmp}/7zip-fixtures.XXXXXX")
trap 'rm -rf "$FIXTURES"' EXIT
export PORT_FIXTURES="$FIXTURES"
python3 - "$FIXTURES" "$ROOT/tests/fixtures/test_read_format_rar.rar.uu" <<'PY'
import pathlib, sys, zipfile, tarfile, stat, binascii, warnings
root=pathlib.Path(sys.argv[1])
warnings.filterwarnings('ignore', message="Duplicate name")
fixtures={'traversal':['../escaped.txt'], 'absolute':[str(root/'absolute-escape.txt')], 'duplicate':['same.txt','same.txt'], 'flat-duplicate':['a/same.txt','b/same.txt']}
for name, paths in fixtures.items():
    with zipfile.ZipFile(root/(name+'.zip'),'w') as z:
        for p in paths: z.writestr(p,'unsafe test fixture')
with zipfile.ZipFile(root/'symlink.zip','w') as z:
    i=zipfile.ZipInfo('escape-link'); i.create_system=3; i.external_attr=(stat.S_IFLNK|0o777)<<16; z.writestr(i,'../outside')
lines=pathlib.Path(sys.argv[2]).read_bytes().splitlines()[1:]
(root/'sample.rar').write_bytes(b''.join(binascii.a2b_uu(line) for line in lines if line != b'end'))
with tarfile.open(root/'many-entry.tar', 'w', format=tarfile.USTAR_FORMAT) as archive:
    for index in range(12000):
        entry=tarfile.TarInfo(f'entry-{index:05d}-日本語-space.txt')
        entry.mode=0o644; entry.mtime=1791000000
        archive.addfile(entry)
PY
mkdir -p "$ROOT/test-results"
if [[ $NO_FOCUS -eq 1 ]]; then
  # Keep native focus assertions in the full suite. During a locked session,
  # run only the explicitly selected independent regressions, without turning
  # the unavailable native checks into skips or successes.
  if [[ -n ${PORT_TEST_CASES_FILE:-} ]]; then
    cp "$PORT_TEST_CASES_FILE" "$FIXTURES/cases.txt"
  else
    python3 - "$ROOT/tests/integration.cpp" > "$FIXTURES/cases.txt" <<'PY'
import pathlib, re, sys
source=pathlib.Path(sys.argv[1]).read_text()
functions=list(re.finditer(r'^    void (\w+)\(',source,re.M))
for n, function in enumerate(functions):
    name=function[1]; body=source[function.start():functions[n+1].start() if n+1<len(functions) else source.index('\n};',function.start())]
    if name not in {'initTestCase','cleanupTestCase','init','cleanup'} and not name.endswith('_data') and 'activateTestWindow' not in body: print(name)
PY
  fi
  CASE_ARGS=()
  while IFS= read -r name; do CASE_ARGS+=("$name"); done < "$FIXTURES/cases.txt"
  echo "Running ${#CASE_ARGS[@]} selected functions; native-focus GUI tests remain pending." | tee "$ROOT/test-results/nonfocus-latest.log"
  if PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/port_tests" "$SEVENZIP_BINARY" "${CASE_ARGS[@]}" 2>&1 | tee -a "$ROOT/test-results/nonfocus-latest.log"; then :; else RESULT=1; fi
  # Include every registered independent suite, including future additions.
  # Only the integrated physical-focus group and AppKit click test need an
  # unlocked desktop; their absence remains explicit, never reported as passed.
  if ctest --test-dir "$BUILD" --verbose --output-on-failure --no-tests=error "${CTEST_ARGS[@]}" -E '^(integration|cocoa_menus)$' 2>&1 | tee -a "$ROOT/test-results/nonfocus-latest.log"; then :; else RESULT=1; fi
else
  if ctest --test-dir "$BUILD" --verbose --output-on-failure --no-tests=error "${CTEST_ARGS[@]}" 2>&1 | tee "$ROOT/test-results/latest.log"; then :; else RESULT=1; fi
fi
"$ROOT/scripts/check-bundle.sh" "$BUILD/7-Zip Mac.app"
exit "$RESULT"
