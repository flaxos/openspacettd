---
name: ost-uat
description: Run or prepare OpenSpaceTTD playtests, reproducible UAT saves, content and terrain audits, or native delivery/recovery acceptance. Use for game evidence and player missions, not ordinary unit-only fixes.
---

# OpenSpaceTTD playable proof

Before authoring or repairing saves, read `docs/SAVEGAME_AUTHORING.md`; it owns
detailed invariants and harness procedures. Preserve original saves and crash
artifacts. Identify binary, save, content versions/hashes, language and UI scale.

Select the existing runner for the claim; inspect arguments before running:

| Claim | Entry point |
|---|---|
| Connected four-world fixture | `scripts/generate_connected_economy.py` |
| Connected legacy terrain/text | `scripts/test_connected_uat_recovery.py` |
| Organic prefab recovery | `scripts/test_organic_uat_recovery.py` |
| Integrated progression | `scripts/test_integrated_economy.py` |
| Natural multiplayer transfer | `scripts/test_federation_multiplayer.py` |
| Economy player checklist | `demo/INTEGRATED-ECONOMY-UAT.md` |
| Broader graphical checks | `demo/HUMAN-GRAPHICAL-UAT-GUIDE.md` |

Fixtures have different topology/content assumptions. Adapt builder, guards and
assertions together; do not invent a parallel construction script.

Fail early: changed NewGRFs → short generation → terrain/foundation and cargo text
audit → representative delivery → long acceptance soak. Include void edges and
every shared corner; legal heights do not prove legal rail foundations. Resolve
cargo by label, preserve native slots, audit all five text fields in the player's
language and English. Never replace published legacy GRF hashes in place.

Use ordinary authoritative construction, vehicle and order commands. Prove repeated
delivery per train/cargo, custody and cash conservation, fresh-process reload and
the interruption/recovery claimed. Storage is not consumption; moving trains are
not deliveries; seeded wealth is not bootstrap balance. Manual federation dispatch
does not prove natural gate-entry multiplayer.

Use diagnostic checkpoints to locate faults; regenerate after builder changes for
cold-start claims. Freeze binary/content before final evidence and retain matching
hashes. Do not repeat a passing long soak without an affected change or new failure.

## A small player mission

Give the launch/save, one objective, 3–5 short steps and a visible success signal.
Aim for a useful observation in about five minutes when simulation timing permits;
declare longer waits. Example: follow one ore train through a gate, watch factory
input rise, save/reload, and see the return service resume.

Ask for pass/fail and the first unexpected event. Offer at most two useful next
missions when the player wants to continue. Record results only for the build/scope
reported in `demo/UAT-RESULTS.md`. Headless, SDL-dummy rendering and human desktop
acceptance remain separate; label pending checks explicitly.
