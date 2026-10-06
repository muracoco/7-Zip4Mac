# Address text aligned with the file list — 2026-10-06

The address combo, editable path field and hierarchy popup now use the actual
file-list font in each panel. No fixed replacement size or system setting is
introduced. Alignment runs after the panel is shown and when its list font
changes. The inherited family/size are made explicit in the copied QFont so
Qt cannot resolve them back to the smaller combo/line-edit class fonts.

A temporary native Qt diagnostic instantiated the actual MainWindow with
private INI settings and measured QFontInfo and QFontMetricsF. On this Mac the
list was 13pt/15.30 logical pixels, the address combo/editor 9pt/10.05, and its
popup 12pt/14.12. The confirmed final run reports the same macOS system font,
13pt/15.30, for all four controls. Both panels share this implementation.

The first constructor-only copy built successfully but did not change the
measured address font. The next show/font-change copy also retained the smaller
font: inherited QFont properties were not explicit. Both ineffective results
are preserved. The final explicit-property copy passed the application build
and runtime measurement. An earlier command used a nonexistent target and
stopped before compilation; the corrected SevenZipMac target was used.

Raw measurements/build logs are retained locally, with their hashes in
[address-font-evidence.json](address-font-evidence.json). This is native Qt
runtime/layout evidence, not a claim of physical desktop clicking. No permanent
implementation-mirroring test or unrelated archive regression run was added.
The prior 0.1.0 distribution is an earlier snapshot without this adjustment.

The packaged application contains source revision
`5aba56d82fc9e9ff64a3031951c494abdae3e97b` and an 807-file corresponding-source
archive. Strict ad-hoc signature and all 15 Mach-O dependency/rpath checks passed.
The actual packaged app was launched on this desktop; its process identity and
window were confirmed, and a fresh screenshot visibly aligns address/list text.
The attempted popup coordinate click was rejected by the UI service after a
window-state change, so popup clicking is not claimed. Its runtime font is
confirmed by the diagnostic above. A separate 807-file publication snapshot
passed local review; no public repository or release was created.
