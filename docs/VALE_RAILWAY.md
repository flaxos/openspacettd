# The Vale Railway

The optional first playable chapter founds a railway company in Aster Vale.
Quarry SILC feeds Stonecross Works; native BALL production is carried to Greybank
Yard. 80 accepted SILC, 40 native batches, 40 stored BALL and two positive exports by
one recurring train with a real Works return establish the railway. Victory
latches once; free play remains. Warehouse freight stores material without sale
revenue. Native SILC delivery can earn ordinary freight income.

## Declared founding assistance

The 256×256 temperate opening contains Greybank, Stonecross, Merefield, public roads,
quarry/works/yard, four short platforms, depot/track stubs, one empty Pioneer with two
SILC hoppers,750,000 GBP,80 imported STEL and Materials I. All SILC/BALL stores start
empty; both through railways are unfinished. Rail fabrication begins off. One exact
Frontier crusher plot is an authored exception; other placement and command gates
remain. Normal New Game generation/selector and absent-chunk saves are unchanged.
No FOOD survival, compulsory Capacity/Reserve, P1 objective or private gate is added.

## Native implementation and bounded tools

- `World0Opening` observes real engine events and saves optional W0OP state.
  Invalid/foreign/deleted/duplicate references and native save reload are tested.
- GSWorld0 queries read saved facts; the story script is optional and persistent.
- `vale prepare` is restricted to a fresh empty editor. `vale bridge` admits only
  that editor or a validated paused named save through an absolute native FIFO.
- `scripts/vale_railway.py` pins exact source/runtime/config/save/receipt hashes,
  unique output/attempt and watchdogs. Limits are 0 preparation/12 service/4 reload
  economy evaluations, 300 seconds per phase and 120 seconds startup. Failed phases
  stop; later phases require successful closed predecessors. No implicit retry.
- `scripts/test_vale_purchase_ui.py` checks a byte-identical supplied save copy,
  uses the real BALL dropdown, row click and Buy handler, retains native screenshots
  and enforces zero evaluations. It never runs against a user's live game.
- `scripts/play_vale_railway.py` and `scripts/resume_vale_railway.py` verify immutable
  runtime/content/config/save receipts before making a fresh player profile.

## Compatibility fixes and dependencies

PR83 owns C0/C1/research fixes. This draft targets its branch and does not duplicate
its diff. The municipal construction policy is a prerequisite of the playable
railwork; all physical, ownership, material, technology and cash checks remain.
Legacy FLIN/development helpers remain inactive support; the previous multi-world
campaign is not revived or claimed accepted.

The modular freight car is an unpowered ELRL wagon. The GUI now matches native
wagon physical compatibility instead of applying a locomotive power predicate.
The real rendered list and paid purchase were checked. LORE reload fixture selects
native base graphics when an inherited video driver exists. Native depot regression
flushes queued signals before pool cleanup, preventing later invalid map references.
Published GRF bytes remain unchanged, including inherited locomotive-style car art.

[Player mission](../demo/vale_railway/PLAY.md) and [engineering record](audit/2026-10-12/vale-railway-publication/README.md).
Human desktop acceptance, enjoyment and pacing remain pending. Private saves,
player progress, paths, raw logs and original implementation history are not published.
