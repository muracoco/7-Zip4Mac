# Finder desktop acceptance — 2026-10-06

Actual Finder right-click → Open With → Other → explicitly selected current
0.1.0 app displayed the 7-Zip operation menu for the owned Japanese/spaced text
fixture. The default-association checkbox remained off. Both quick compression
buttons completed and produced sibling 7z (185 bytes) and ZIP (223 bytes) files.
Each resulting archive passed Test and extraction with the bundled official
26.03 engine; restored SHA-256 matched the original 43-byte file. These latter
checks were engine invocations, not Finder-menu GUI Test/extraction claims.

Finder's generic “7-Zip Mac” candidate instead launched a retained October 3
build with the same bundle identifier and 0.1.0 version. Its old manager UI and
exact process path confirmed the mismatch. Only that newly launched owned process
was closed through its window; the updated app stayed separate. Explicit current
app selection for the ZIP also showed a disabled Open button, even with All
Applications. Refreshing only the current app's Launch Services registration did
not resolve it; no default handler or unrelated registration was changed.
A read-only LSCanURLAcceptURL check returned status 0/accept=true for the current
app and txt/7z/ZIP, so handler capability alone does not prove Finder selection.
Transient cgWindowNotFound observations were followed by successful Finder
acquisition; they are not treated as permanent application failures.

The candidate now uses project version 0.2.0, and both bundle version keys and
the About port version derive from that single CMake value. This separates it
from the retained 0.1.0 development copies without changing its bundle identifier
or preferences. This is distribution preparation and a routing investigation;
it is not yet evidence that the disabled Finder Open button is fixed. The
application and affected dialog-resources target built. The existing dialog
suite passed once (2.43 seconds); generated metadata still declares all 138
extensions. Engine/format code and imported Windows bodies were unchanged, so
passing unrelated suites and the 151-format matrix were not repeated.

## Subsequent 0.2.0 desktop results

The packaged `b8daa32` candidate's actual About dialog showed port 0.2.0 and Qt
6.11.3. Finder's ZIP context menu listed 0.2.0 separately from the older 0.1.0
copy. Selecting that 0.2.0 entry delivered the operation menu to the exact
current app process. Its Test command reported one archive, one file, 43 bytes,
223 packed bytes and no errors. Extract to created a previously absent child
directory; its restored file matched the source SHA-256. The last File Manager
button opened the ZIP listing; Backspace returned to the parent filesystem.
These are actual Finder/GUI observations, distinct from the earlier console
verification. They establish the current versioned candidate route, without
claiming that the disabled Other chooser's underlying cause was resolved.

The UI service rejected the subsequent coordinate right-button event before
delivery (`noWindowsAvailable`). The native AppKit right-button/Options tests
and Qt context/drop workflows remain separate evidence. Physical Finder drag,
hardware modifier input, printer hardware and a Windows pixel comparison are
unverified environment/visual observations, not missing portable command bodies.
No unrelated process, OS security setting or default association was changed.

During this verification, an owned extracted file's timestamp anomaly was
reproduced independently and repaired as recorded in
[zip-omitted-times.md](zip-omitted-times.md). Conditional public publication
has not occurred. Remaining packaging/publication work uses the repaired source.

Apple documents the bundle identifier's role in choosing app copies and the
heuristics used when several apps share it: [Core Foundation keys](https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Articles/CoreFoundationKeys.html)
and [NSWorkspace application lookup](https://developer.apple.com/documentation/appkit/nsworkspace/urlforapplication%28withbundleidentifier%3A%29).
The specific reason for the disabled chooser button is still unproven.
