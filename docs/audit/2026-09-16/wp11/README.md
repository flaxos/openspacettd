# WP-11 structural economic slice — 16 September 2026

Implemented locally on `fix/wp11-content-economic-slice`, based on `3081af218d`.
Graphical human acceptance remains pending. This accepts the structural chain;
it does not accept the other production pipelines, federation or a finished art pack.

## Changes

- The two version-2 packs define 13 distinct cargo labels, 14 loaded industries
  and 12 train definitions. Freight wagons have explicit refit sets and zero power;
  locomotives have no inherited passenger capacity. The iron mine uses the native
  iron-mine substitute. Source, binaries and manifest pass pinned NML verification.
- Recipe inputs/outputs and fabrication roles share the loaded cargo binding.
  No-pack games retain their existing vanilla aliases. Missing, disabled, old or
  compatible-replacement configured packs and missing/duplicate/conflicting labels
  reject Commonwealth construction; existing processing buffers remain untouched.
  Recipes bind after NewGRF loading finishes. Saved cargo IDs are not remapped.
- Materials I gates the station furnace and steel-based fabrication, including
  Blueprint query/execute paths. CST purchase failures distinguish research and
  world phase. Materials III retains its 90% material-cost discount and 15% yield.
- A bounded offline fixture builds native industries, stations, portals, trains,
  orders and a hub. Native simulation ticks supply all ore and steel. Read-only
  observers reconcile actual native cash entries, industry allocation and station
  rating losses. Reservations are counted once (station totals plus stored cargo
  aboard trains). The fixture command refuses games with connected clients.

## Verification

- Incremental Ninja build; **377/377 CTests passed**.
- `python3 .github/unused-strings.py`: **OK**.
- `python3 scripts/build_commonwealth_grf.py --verify` with pinned NML: **passed**.
- Active-content acceptance passed at **1800 and 2300**. Each run validates loaded
  labels/catalog/refits and queries an actual Vulcan purchase before research,
  after unlocking Traction I, and on a forbidden world phase.
- Each run starts with only **5 ballast** in inventory, startup construction cash
  and Materials I. One native mine feeds an ore train through a local portal to
  the station furnace; a second train carries its steel through a second portal
  to a Core-world hub. No ore or steel is injected.
- A loaded ore train is saved mid-route and reloaded in a new process. Cargo held
  by every station, both trains, facility buffers, stockpile, transit state and cash
  match. A removed steel-route track prevents deliveries for 35 advance blocks;
  after repair, three complete 40-unit loads deliver **120 steel**.
- All 65 advance blocks reconcile cargo transformations/disposal and native cash
  debits/revenue. The final connected Core depot consumes exactly **10 steel and
  5 ballast** once. Quote and execution both charge **120** versus the cash quote
  **540**: 80% off the 525 rail/depot component, plus unchanged 15 clearing cost.
  Final stock is **110 steel, 0 ballast**. These are engine money units; display
  currency can apply its own multiplier.
- Unit tests cover malformed active content, retained buffers, company/world
  stockpile isolation, locked furnace/Blueprint atomicity and Materials III's
  40 ore → 23 steel conversion and 90% discount.

[Compact evidence and artifact hashes](evidence.json).
[Versioned saves and graphical steps](../../../../demo/WP11-UAT.md).
Full local logs and per-block ledgers are under `build/wp11-release-1800/` and
`build/wp11-release-2300/`; Linux CI now runs both dates and uploads its evidence.
The workflow changes have not yet run on GitHub.

## Reproduce

```sh
ninja -C build
python3 scripts/test_wp11_slice.py --year 1800 --output build/wp11-check-1800
python3 scripts/test_wp11_slice.py --year 2300 --output build/wp11-check-2300
```

Requires an installed OpenGFX base set and permission to bind a loopback socket.
Each engine process is limited to 120 seconds, with 45-second command timeouts;
advance counts are bounded. Every run starts a fresh map. Existing saves,
including the old no-pack UAT fixture, are preserved. Do not add these packs to
an existing no-pack save or accept a GRF replacement for a different saved hash.
