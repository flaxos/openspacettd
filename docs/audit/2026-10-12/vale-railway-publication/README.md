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

The clean publication branch has one new implementation commit on the dependency
head. Original local commits, private proofs, game files and Antigravity checkout
remain intact; no rewrite, force push or merge is performed.
