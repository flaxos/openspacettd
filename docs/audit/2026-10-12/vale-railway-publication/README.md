# Vale Railway engineering verification

The published source contains the optional Aster Vale founding scenario, its native
persistent observer and GameScript queries, bounded authoring/proof tools, required
construction behavior, and the confirmed freight purchase and test-isolation fixes.
It is stacked on PR83, which owns C0/C1 and material research. This change does not
add a P1 objective or accept the broader Reserve/FOOD pilot.

The prior local implementation passed one empty-stock preparation, native service
proof and separate process reload (0/9/4 economy evaluations). Authored assistance
is declared in the story and [player guide](../../../../demo/vale_railway/PLAY.md).
Native delivery, subsequent production, positive warehouse deposits and a real
source return were observed. Warehouse ballast storage earns no sale income.

The purchase defect hid an unpowered ELRL wagon in a conventional RAIL depot by
applying the locomotive power predicate. The list now uses physical rail
compatibility for wagons and retains powered locomotive restrictions. The actual
SDL-rendered screen showed the selected Commonwealth Modular Freight Car under
Ballast and Concrete, with capacity 40. The real Buy handler charged 93 GBP for one
empty wagon; existing fleet/materials/objectives were unchanged and the simulation
advanced zero evaluations. The first probe omitted normal row selection and stopped
before purchase; it was corrected and the failure is retained privately.

The aggregate LORE reload crash occurred in GfxLoadSprites after an earlier test
left a video driver without native base graphics. Its fixture now supplies the
prerequisite and uses an exclusive save directory. The purchase regression flushes
its signal buffer before removing its depots, preventing stale map references in
later ownership tests. Existing native assertions remain intact.

[Sanitised verification record](verification.json) contains engineering outcomes.
Private saves, raw game receipts/logs, personal paths, financial balances, actual
player progress, screenshots and original implementation ancestry are excluded.
The original private evidence remains preserved locally; it is not replaced by
this summary. Publication integration and CI are distinct from prior local proof.
Human desktop appearance, comprehension, enjoyment and pacing remain pending.
The modular wagon still inherits locomotive-style art; published GRFs are unchanged.

## Publication integration checks

Replayed source onto PR83's corrected head with its research guards and tests
preserved. Both native targets build; aggregate Catch 631 cases / 310,772 assertions,
652 isolated CTest entries (including four localhost relays) and 57 offline helper
checks pass. Four unrelated game-running regression entries remain excluded from
local checks; remote CI exercises its normal suite. File descriptions, unused
strings, script mode enforcement and upstream diff validation pass. Independent
staged privacy/scope review is GO. Exact-head CI is pending at publication.

The clean publication branch has a new implementation commit with bounded CI repairs on the
dependency head. Original local commits, private proofs, game files and Antigravity checkout
remain intact; no rewrite, force push or merge is performed.

## CI repair validation

Initial CI exposed missing source documentation, a macOS `Boolean` type collision,
an MSVC JSON/string-view overload ambiguity and an assertion-enabled demolition
fixture that attempted replacement without clearing the original house tile.
All 409 introduced warning signatures are cleared by matching Doxygen 1.17.0;
new native modules have no warnings. Local parsing uses Autogen output to conserve
disk, with the repository's warning settings retained. Remote CI still runs its
unchanged documentation workflow.

The two private JSON helper names are now `JsonBoolean`. The funding test extracts
a string before comparing it. The demolition fixture initializes ordinary town
counts and calls native `ClearTownHouse`, verifies a clear tile, then keeps the
original invalidation and replacement-never-win assertions. Independent review
confirms the bounded changes and no weakened assertions or checker suppressions.
Both native targets rebuild; 35 focused cases / 957 assertions, aggregate 631 cases /
310,774 assertions, 648 isolated nonrelay CTest cases and four localhost relays pass.
Exact repair-head CI remains pending when this record is committed; the PR records
its subsequent terminal outcome without replacing the original failed results.


The next CI run exposed `std::jthread` availability below macOS 11 in the native
FIFO fixture. The fixture now uses supported `std::thread` and joins on both normal
return and exception paths, rethrowing the original exception. Retry and I/O bounds,
cleanup and assertions remain unchanged. Independent review is GO; the rebuilt
fixture passes all six isolated startup/completion checks and the full local suite.
The macOS deployment target and CI configuration are preserved.


The assertion-enabled macOS test run then exposed two fixture prerequisites. Its
`/tmp` path has a symlink ancestor, which production admission correctly rejects.
The exclusive FIFO fixture now uses its canonical directory; an added negative
case verifies rejection of a symlink ancestor. Production admission is unchanged.
The unchanged hub pickup reload fixture separately failed to create a timestamp-named
directory before SaveOrLoad. Its bounded exclusive creation now preserves existing
candidates and claims only a directory it creates itself. A parallel timestamp
collision explains the observed failure but is an inference, not a measured cause.

Independent review approves both fixture-only repairs. Rebuild, eight focused checks,
aggregate 631 cases / 310,776 assertions, 648 isolated CTest cases and four localhost
relays pass. No native gameplay proof or owner save is rerun or replaced. Exact new
revision CI is recorded subsequently on the PR.


The follow-up head `e115d4ca1d` passes all platform builds/tests and static workflows.
Its final annotation gate reports one MSVC width conversion in the fixed three-town
fixture. Using a `uint` loop index keeps coordinates 20, 40 and 60 and matches native
`TileXY` without narrowing. Independent review and the rebuilt fixture/full local
suite pass. The annotation gate is retained; exact updated-head CI remains required.
