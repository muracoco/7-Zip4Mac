# Archive refresh after official Agent updates

The update operation is still the imported official 7-Zip 26.03 Agent body.
This increment adapts its verified result to Qt rows; it does not replace the
archive engine or implement another Rename/Delete algorithm.

The Windows reference is `CPP/7zip/UI/FileManager/PanelOperations.cpp`,
`CPanel::OnEndLabelEdit`, and `PanelItems.cpp`, `SaveSelectedState` /
`RefreshListCtrl`: retain the operated folder, selection and focus, change the
focused name after a successful rename, then reload. Qt cannot import their
Win32 ListView/control calls unchanged. The existing verified handler-index
map is now extended with the original Agent proxy directory and row identities.

- Explicit folders bind through their mapped real handler owner. Implicit
  folders bind through the verified renamed prefix and native graph; ambiguous
  directory paths require surviving descendant identities.
- Rows retain `(directory, local item)` identity. Index `-1` no longer selects
  every implicit folder. Normal and Flat views preserve their bound base folder.
- A deleted bound folder returns to a surviving graph parent. The displayed
  address uses that actual folder, rather than a stale prefix with root rows.
- Pending editor and nested-parent selections retain the updated base path.
  Nested virtual addresses and editor locations follow an ancestor rename.
- Surviving selected rows are restored after Delete; a removed focus falls
  back to the previous visible position, bounded by the new row count.

## Executed checks

Mac: macOS 26.6.2, Apple M3 / arm64, Qt 6.11.3, official 7-Zip 26.03.
The affected Qt app/test targets compile. The final scoped run passed **26
checks including setup/cleanup**, with no failures:

```bash
./scripts/test-agent-tree.sh /path/to/build agent_selection_tests \
  guiAncestorFolderRestoration guiSameNameRenameAndDelete \
  guiSameNameEditorWriteBack nestedParentRenameWriteBack \
  nativeUpdatesAcrossWriters implicitFlatUpdatePolicies \
  singleStreamUpdates multiImageWimNativeSelection
```

The new cases use genuine implicit ZIP folders and explicit 7z folders,
normal/Flat two-panel ancestor Rename/Delete, an actual gated external editor
followed by verified extraction, and a nested ZIP inside a renamed 7z ancestor
followed by verified outer/inner extraction. Existing duplicate-name, TAR/WIM,
prefixed 7z/ZIP and official single-stream cases also pass.

An intermediate test run incorrectly rediscovered the second panel's address
widget when asserting the first panel's path (both have the same object name).
The test now retains each panel's address widget before creating the other
panel. That failed run is not counted as a passing verification.

[Final execution log](archive-refresh-test.log). Physical desktop clicks and
Finder/Fn-key checks remain unverified. This scoped graph fix does not complete
the remaining extraction/overwrite, UI/Help/list, refactoring, final clean build
or publication requirements. The complete format matrix and unrelated suites
were not repeated for this UI refresh change.

The normal `.app` was incrementally rebuilt and packaged. Dependency and
ad-hoc signature checks passed for all 15 Mach-O files. Its executable survived
startup on the actual Mac with an owned Japanese/space-name fixture and private
preferences, without development Qt loader/plugin paths. The bundled official
engine exited zero. Only that owned verification process was stopped. This is
bundle/startup confirmation, not physical native-menu or Finder verification.
