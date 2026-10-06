# Maintenance verification, 2026-10-06

Scope: the supplied license findings, incremental upstream import support and
source/app distribution cleanup. Existing archive algorithms and imported C++
behavior remain unchanged; the About version/date/copyright now come from the
shared upstream lock.

## Executed

- Official 26.03 archive and all 1,292 source-file hashes verified.
- All 55 regenerated C++ adapter bodies matched the prior source exactly after
  removing the three new modification-notice lines. No original license header
  was removed. The 15 helper overlay files retain original headers and CRLF.
- Seven focused update/profile checks passed: identical candidate, isolated
  staging, synthetic next-release pins, affected importer detection and rejected
  changed anchor, bad checksum/traversal/absolute/link/duplicate paths, optional
  build source profile. Synthetic archives are explicitly local fixtures.
- Native helper rebuilt on Apple Clang: metadata formatter 11 passed, Benchmark
  formatter four passed.
- A Git-free build-source archive with no Port tests/fixtures/logs was extracted
  and built from an empty CMake/Ninja directory. BUILD_TESTING=OFF produced no
  developer test binaries; Qt Test was not required.
- That .app started on the real macOS desktop, displayed the filesystem root,
  and its About dialog showed upstream 26.03, Port 0.2.5 and Qt 6.11.3.
- Ad-hoc signature and bundled runtime dependencies passed packaging checks.

- The affected compression/Help GUI suite passed (one CTest group, 7.68 seconds).
- Final reviewed public development snapshot: 683 regular source files. The app
  build-source archive has 516 files (five documentation files) and contains no
  Port tests, fixtures, raw logs or evaluation documents. It is approximately
  1.02 MiB compressed. All member hashes and normalized file modes are checked.
- The final 516-file Git-free source profile also rebuilt from a second empty
  CMake/Ninja directory. Its package passed the distribution guard and strict
  ad-hoc signature/system-only dependency checks: 15 Mach-O files, four runtime
  executables, six QtBase frameworks, no Qt Test framework.
- The final packaged helper completed 7z and ZIP Add/Test/Extract round trips:
  Japanese/spaced paths, nested files and an empty directory; SHA-256 matched.

Raw logs are kept locally in the maintenance work directory instead of being
published or bundled. The final package receives this verified source snapshot
and notices before installation.

## Reproduce focused checks

```bash
python3 tests/upstream-maintenance.py
PORT_BUILD_TESTS=ON ./scripts/build.sh
PORT_TEST_REGEX='^compression_help$' ./scripts/test.sh
python3 scripts/check-distribution.py /path/to/7-Zip\ Mac.app
./scripts/check-bundle.sh /path/to/7-Zip\ Mac.app
```

No exhaustive format rerun, Intel/macOS 15 verification, Windows pixel comparison,
future official release compatibility or Release/binary upload is claimed by
this maintenance change. The earlier tested runtime feature results remain in
the component documents.
