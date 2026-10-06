# License audit follow-up, 2026-10-06

The supplied audit examined public commit
`d187c4d22f2e220bf87faadd3e173560292fbef2` (0.2.3). Its two concrete findings
also applied to the subsequent 0.2.4 source; those are the scope of this repair.

1. All generated upstream-derived C++ adaptations now identify the modifier as
   the 7-Zip4Mac contributors, record actual first/latest adaptation dates from
   Git history, and date the added notice to 2026-10-06. For compression these
   historical dates are 2026-10-04 through 2026-10-05; for the helper overlay they
   are 2026-10-04 through 2026-10-06. The generator adds the notice on every
   regeneration. Existing original notices, C++ bodies and overlay CRLF/BOM are
   retained. Future substantive modifications must update the affected group's
   date in the lock.
2. `licenses/NOTICE.md` now distinguishes original source-built `7zz` from
   modified `7zz-progress`, including Common extraction/update callbacks, Agent
   proxy changes and ZIP update metadata, rather than claiming an unchanged
   engine. The original 7-Zip license texts are unmodified.

The modification/date requirement is described in
[LGPL 2.1 section 2(b)](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html).
Component terms are retained as described by the
[official 7-Zip license](https://www.7-zip.org/license.txt).
This repair does not broaden the audit into a trademark opinion or certification
of an unproduced binary Release. No Release or binary upload was performed.
