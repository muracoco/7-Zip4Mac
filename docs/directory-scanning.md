# 後続更新

表示文字列の遅延取得・大規模sort比較・native icon取得は後続の
[panel-listing-port.md](panel-listing-port.md)で更新しました。以下の同期sort／icon記載は
当時の結果です。最終model挿入／並べ替え反映・解析の同期処理は現在も残ります。

# Filesystem and Flat View loading

`DirectoryScanner` reads directory entries and their displayed metadata on a
Qt Concurrent worker. Each request has a generation and cancellation flag.
Workers capture data only; replacing a request or destroying a panel cannot
deliver an obsolete result to the GUI or wait for that worker on the GUI thread.

Enumeration includes hidden/system entries, empty folders and Unicode names.
Flat View recursively opens child directory descriptors with `O_NOFOLLOW`,
checks the opened directory identity and never traverses child symlinks.
A user-selected root symlink remains a browsable location. FIFO entries are
listed without opening their contents. Read failures retain the path and errno;
partial candidates do not replace the displayed list. Error warnings open
asynchronously: a worker-completion callback never enters a nested dialog
`exec()`. Its failure notification completes when the warning is dismissed.

The GUI builds detached rows in batches of at most 128 entries / approximately
8 ms, then installs the completed candidate. The previous complete list,
selection, header and committed navigation settings survive cancellation or
failure. Only the latest successful request records LastPath / History, starts
watching its location and runs selection-restoration continuations.

Refresh, archive-root Up, Options Apply and Open With → File Manager restore
selection after installation. Reading both panels simultaneously is allowed.
Address navigation, Up, Refresh and Favorites may supersede a read. Escape
cancels the panel's read while its window/address control receives the event.
Removing a second panel and closing a window invalidate pending results.
Finder/Open With delivery cancels hidden startup scans so the operation menu
does not wait for a large remembered location; showing File Manager restarts
any cancelled, never-rendered panel.
Mutating commands remain disabled until both panels have finished reading.

`directory_scanner` tests real dedicated temporary folders and isolated
preferences: normal/Flat metadata, links and cycles, 30,000 hard-linked entries,
GUI heartbeat, cancellation during row preparation, superseded requests,
scanner destruction, repeated Options Apply, Refresh/manager selection,
permission failure/recovery, archive exit/Refresh/Up, header restoration and
two-panel creation/removal/close. See [executed results](test-results.md).

The final Qt model insertion/sort/old-row deletion is still synchronous.
Native icon lookup stays on the GUI thread, although metadata is prefetched.
Single root-type checks used to choose filesystem vs archive navigation and
kernel I/O cannot be forcibly interrupted. Therefore this does not claim
bounded latency for arbitrary network mounts, native icon providers or
unlimited entry counts. Archive rows use their existing separate rendering
path. A fully virtual list model remains a useful performance improvement.
Physical desktop/Finder/Fn/menu tests still require an unlocked session.
