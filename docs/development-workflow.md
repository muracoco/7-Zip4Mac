# Port implementation workflow

Use the pinned official Windows source as the implementation baseline. Complete a related feature group, including its backend, Qt controls and documented differences, before running that group's functional tests. A missing feature is recorded as an implementation gap; a failure in a completed path is recorded as a defect. OS-specific differences remain a separate category.

Build the group and run its principal workflows and realistic failure cases together. After repairs, rerun failed cases and cases affected by the change. Repeat broader checks only when a change or unresolved failure justifies them. Do not interrupt each small edit with a fresh full regression run or add speculative combinations while required features remain unfinished.

Record what was implemented, automatically checked and physically checked separately. Preserve initial grouped failures and targeted reruns in the same execution record. Milestone commits preserve a usable state. Final clean-build, bundle, all-format and available desktop acceptance checks belong at the release checkpoint.

Use the current implementation and the latest integration checkpoint to decide
what is missing. Older component reports are historical evidence, not a current
backlog. Close a batch against source-defined behavior and concrete failures;
do not keep it open solely because every possible archive variant has not been
enumerated. Keep unavoidable physical checks pending while the desktop is locked,
and continue the independent release work.
