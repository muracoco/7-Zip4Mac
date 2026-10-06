# Public source publication — 2026-10-06

The reviewed source is public at [muracoco/7-Zip4Mac](https://github.com/muracoco/7-Zip4Mac).
The repository has English project material, a Japanese README and the requested
bilingual description. It is an unofficial macOS port, not an official 7-Zip
product. The audited portable source groups and tested local 0.2.0 package are
complete; documented safety differences and hardware/pixel limits remain.

## Verified publication

The first complete public commit is
`10adfffed58859258f4b64058166e379d97441c0`, tree
`b9dd8f024bfd85c4a617e6ccfd5f014760537d33`.
All 812 reviewed file paths, bytes and Git modes matched the local public source
snapshot after an independent anonymous Git fetch. Publication uses a separate
English history; internal development history, private diagnostic paths,
credentials and generated build data were not uploaded.

Repository creation initially required browser authentication. After the user
signed in, creation succeeded. The ordinary Git CLI push then failed because
CLI credentials were unavailable; that failure is retained separately from the
successful publication. The connected GitHub API transferred the reviewed tree
and updated `main` without force. No GitHub Actions, tag, Release or paid service
was used. Later closeout documentation is separate from the initial tree above.

## Package and executed verification

The local signed arm64 0.2.0 package was built from an initially empty Ninja
build (252 tasks), using official 7-Zip 26.03 and dynamic QtBase 6.11.3.
The application implementation revision is
`734235ca1ebe60283a2e73a1e76d74013ec91c9a`; later closeout edits are documentation.
The ZIP has 98,622,223 bytes, SHA-256
`0b8eb965012e660c48cada027189b2b256c67fd0a19c13c738cd94c212de224c`.
Its 812-file corresponding-source snapshot predates the repository-link and
closeout documentation. Original source/licenses and all compiled runtime files
are unchanged by these publication edits. The local ZIP is not a GitHub Release
asset or a public binary download.

CRC, unpacked source hashes/modes, ad-hoc signatures, 15 Mach-O dependency checks
and isolated startup passed. The address field/list/popup use the same resolved
13pt font on the tested Mac. Actual Finder Open With compression, GUI Test and
extraction, Japanese/spaced ZIP listing and final-app extraction were observed;
restored content hashes matched. A reproduced ZIP zero-timestamp defect was
repaired; the complete affected extraction suite passed 323/323 Qt cases, and
the fresh bundled regression passed 3/3 including setup/cleanup.

The initial 35-suite run, affected repairs and all 151 handler/extension
registrations are recorded in [release-consolidation.md](release-consolidation.md).
They are not represented as one uninterrupted green run. Initial OS input-method
stderr, test-driver failures and the exact passed follow-ups remain in
[release-0.2.0.md](release-0.2.0.md) and its evidence record.

## Retained limits and next improvement

Physical Finder drag, hardware Fn/RightCtrl/Option, real printer output,
macOS 15/Intel hardware and exact Windows pixel equality remain unverified.
The earlier disabled Finder Other chooser is not claimed fixed; the explicit
versioned 0.2.0 route worked. Finder Extension/Quick Action/Services remain
optional unimplemented integrations; the requested configurable Open With menu
and permanent final File Manager entry are implemented.

Port-only English text, guarded filesystem/comment/update behavior and final
large-list model insertion/metadata parsing still differ as documented in
[windows-parity.md](windows-parity.md) and [current-status.md](current-status.md).
The most useful next improvement is desktop interaction and visual comparison
of the remaining physical drag/modifier cases, followed by the observed gaps;
passed engine/format groups do not need speculative repeat runs.

## Reproduce

```sh
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/test.sh
./scripts/run.sh
```

[English README](../README.md) and [日本語README](../README.ja.md) describe build
paths, prerequisites, clean-build commands, test selection and license notices.
