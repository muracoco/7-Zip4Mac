# Finder registration — 0.2.2

## Cause and policy

Finder listed multiple versions and test bundles because every build declared
the same bundle identifier and Viewer / Alternate document claims. Launch
Services records applications outside `/Applications`, including explicitly
opened cache/build copies. Choosing a newer version does not unregister them.

Build and package copies now retain their supported document types but declare
`LSHandlerRank=None`. Only the installed copy declares `Alternate`. Apple
documents the ranks in [Core Foundation keys](https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Articles/CoreFoundationKeys.html).
The installer uses targeted `lsregister -u/-f` operations; it does not reset the
Launch Services database, restart Finder or change any default application.

```bash
./scripts/build.sh
./scripts/install.sh
# Or specify the build and destination:
./scripts/install.sh /absolute/build/path "$HOME/Applications/7-Zip Mac.app"
python3 tests/finder-registration.py
```

The installer checks the source signature/runtime dependencies, stages a copy
beside the destination, enables its claims and re-signs it ad hoc, then replaces
the installation at the same path. A newer installed version cannot be replaced
by an older source. A running target must first be closed. Permission failure
does not trigger sudo or alter system permissions; use the user-local destination
if `/Applications` is not writable.

Existing same-ID copies under the explicitly managed source/cache roots are
made non-candidates as well. No unrelated application is modified. Other users'
independent installations, unmounted volumes and arbitrary unmanaged copies are
outside this operation. Keep using the installer for updates. A loose package
copy dragged manually to `/Applications` retains its non-candidate rank until
installed; the app can still be launched directly.

## Retained originals

The previous installed bundle remains in the installer's hidden staging
directory. Development/old bundles retain engines, Qt, licenses and source
archives. Before altering a historical bundle's plist and outer signature, the
script verifies a `.7zip-finder-backups/*.tar.gz` copy of the original plist,
main executable and resource seal. Nested frameworks/plugins are not re-signed.
The install receipt prints both rollback paths. No user archive, extracted file
or preference is deleted or changed.

To recover the original bytes of a retained bundle:

```bash
python3 scripts/finder-registration.py restore /path/to/previous.app /path/to/signing-backup.tar.gz
```

Restoring the original metadata can make that old app a Finder candidate again;
use this only for intentional rollback/debugging. Historical acceptance records
describe the original signatures before this migration; their preserved signing
backup restores those bytes. Rebuild scripts generate the new non-candidate
plist, and package.sh enforces it before signing.

## Verification

The affected test uses dedicated signed fixtures and real Launch Services APIs.
It checks build suppression, single-candidate installation, explicit rescanning,
unrelated app retention, byte-identical rollback/signature recovery, and rejected
downgrades. Its fixtures are unregistered after the test. The OS temporary
directory cannot model Finder registration: Launch Services omits applications
there from document candidates, so the test uses an owned cache-local directory.

Archive algorithms and extraction/compression UI have not changed; their full
format matrix is not rerun for this metadata/install change.

On macOS 26.6.2 / arm64, the actual 7z document had **21 port candidates before,
1 after**. Migration covered **48** managed build/verification copies. The
installed `/Applications/7-Zip Mac.app` is 0.2.2. Actual Finder right-click → Open
With visibly lists one `7-Zip Mac`, without old-version qualifiers, font-package
or previous-installed entries. Selecting it opens the correct 7-Zip operation
menu; cancellation exits normally. Archive Utility remains the default.

The eight affected registration/install checks pass. Installed bundle startup
passes with no stdout/stderr, private test settings and unchanged native user
preferences. Signing and 15-Mach-O dependency checks pass. This reuses the
existing build for a plist/install/Help-version update, not a new full clean
build or a rerun of the archive matrix.

The first migration stopped on an old incomplete ASan build whose executable
had never been produced. The script now preserves and hides that plist without
trying to sign a missing executable, and recognizes only Launch Services'
specific `kLSApplicationNotFoundErr` (-10814) as an already-absent registration.
The affected test includes that case. Original failure and successful recovery
logs are retained locally. Earlier test-driver attempts in the OS temporary
directory, and a fixture copying protected flags from `/usr/bin/true`, did not
model registration correctly; cache-local fixtures copy bytes and executable
permissions instead. Those were fixture corrections, not application failures.

[Machine-readable evidence](finder-registration-evidence.json) contains result
categories and local-log SHA-256 receipts without publishing local paths/logs.
