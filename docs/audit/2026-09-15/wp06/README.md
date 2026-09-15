# WP-06 — safe Blueprint imports and library storage

**Implemented and verified locally, 16 September 2026 (Sydney).** User goal
`sp-06` was interpreted as the recommended WP-06 recovery package. The existing
build was reused with `-j2`; both executables are rebuilt. No player library or
save was changed. [Exact checks and hashes](checks.json), [source delta](source.patch),
[separate parser/storage reviews](source-review.json).

## Result

- Strict version1 JSON admission checks integer types before narrowing, enum/bit
  ranges, shapes, duplicate keys/cells/signals, rail/signal compatibility and valid
  UTF-8 metadata. Invalid in-memory models cannot reach rotation/mirror helpers.
  Bounds:2 MiB JSON,16 containers deep,64x64/4096 cells,256-byte names,4096-byte
  author/description. C0/DEL text controls are rejected. Valid existing v1 exports
  remain supported; rejected files stay on disk.
- Rows retain their actual backing filename. Save/update/rename write an exclusive
  sibling temporary file, check write/flush/close, then atomically publish it.
  Rename updates metadata inside the same file. Failed operations retain the old
  bytes and in-memory row. Deletion reports failure instead of dropping the row.
- New sanitized-name collisions receive suffixes; ambiguous existing player names
  cannot be updated implicitly. Import rejects a duplicate player name and export
  refuses an existing destination. Symlink files and oversized input reject.
- Import/Export now open native file-path queries and perform real file I/O.
  Export uses the current rotated/mirrored selection. Default exports live outside
  the scanned library, preventing implicit duplicate imports on reopen. Library
  scan, rename, delete, import, export and capture-save errors are reported.

## Verification

| Check | Result |
|---|---|
| Blueprint/CST CTests |30/30 pass:18 existing +12 new parser/storage/GUI cases |
| Related Sprint28 CTests |5/5 pass |
| Existing prefab placement combinations | All128 rotations/mirrors/payment variants still pass |
| Actual native Import/Export/Rename query + OK callbacks |3 isolated GUI cases pass, including invalid signal255, cancellation, collision, reopen, transformed export and scan-error reporting |
| Real injected file operations |6/6 pass: open, partial write then failure, fsync, close, replacement rename and new-file link publication |
| Failure preservation | Each injection reached exactly once; old bytes and memory unchanged, reopen matches, retry succeeds, no leftover temporary file |
| Build and executable help | Exit0; existing build220 MiB, free space3.5 GiB |
| Registered CTests |351; the inactive I/O worker stub is excluded from the35 selected test count |

The Linux fault runner compiles a temporary interposer and launches one isolated
test process per fault. It is never linked into the game. Every fixture uses its
own temporary library; the legacy storage test was also corrected to do so.

### Failures retained

Initial compilation caught missing GUI declaration includes. Initial tests passed
26/30: a collision fixture assumed insertion order after sorted scanning, and
three query tests lacked a video driver for editbox focus. These fixture issues
were corrected. The query tests use a null driver in separate CTest processes;
their hidden Catch tag prevents driver state leaking into the default unit suite.
Old CMake test-name quoting initially misregistered those three cases; explicit
underscore names corrected registration. Final35 selected tests pass.

No production assertion was weakened. Existing Release/NDEBUG limitations remain
recorded as OST-TEST-001. No full-suite, Windows execution, graphical screenshot,
power-cut recovery or multiplayer claim is made by these checks. Atomic file
replacement and checked file flush are exercised; power-loss filesystem durability
has not been fault-tested.

## Player check and next package

On rebuilt `build/openttd`, select a built-in without an existing player copy,
optionally Rotate/Flip, then Export to a new file. Close/reopen the library and
Import that path. Compare the layout, rename the editable imported row, and
close/reopen again. Existing destination or duplicate player name must show an
error and preserve the old file. Detailed steps: UAT-04f/g.

Graphical acceptance remains pending. Capture drag/naming and advertised prefab
routes remain **WP-07**, the next package. No capture/topology redesign or automatic
conversion/deletion of an existing library was included.
