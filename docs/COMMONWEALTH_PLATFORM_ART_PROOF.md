# Commonwealth platform art proof — 1 October 2026

The first visual delivery is a separate optional four-sprite platform family,
with normal 8bpp fallback and normal/2x 32bpp art. It establishes an original
industrial palette and a reproducible content pipeline while gameplay and CI
continue independently. Human visual acceptance remains **Pending**.

[Automated evidence and boundaries](audit/2026-10-01/platform-art/README.md)
cover the admitted static pack, paired native freight/reload, both axes,
transparency and 8bpp/32bpp SDL captures. Full-save byte equality is not claimed.

## Design and boundaries

Use quiet graphite seams and blue-grey steel decks, pale ceramic safety edges,
restrained amber hazard ticks and small cyan service accents. Company paint has
separate inset panels. Repeated four-unit panels provide a modular scale cue;
clear rails, train wheels and signal aspects take priority over close-up detail.
The production source is original parametric geometry under GPL-2.0-only.
Generated concepts are non-production references, without copied logos or text.

The optional `OST\06` v1 pack replaces native IDs 1069–1072. It preserves their
anchors and 16×5×2 / 5×16×2 sorting bounds. A compiled allowlist permits package
metadata and Action A only. The replacement applies globally to those IDs,
including open strips beside untouched native shelters/buildings. It is not a
station-wide or world-wide reskin. Details, hashes and the build contract are in
[the source package](../pkg/commonwealth_platforms/README.md).

Existing NewGRFs, terrain attempts and published saves remain intact. No engine
rendering, simulation, economy, physics, station footprint, callback, vehicle,
save-format or network change is part of this proof. 4x sprites and a global UI
theme are deferred. Static admission and save removal must be proven on this
fork; they do not exempt art from federation content manifests or multiplayer QA.

## Parallel ownership

| Owner | Work that can proceed now | Interface / exclusion |
|---|---|---|
| Visual track | This platform family, reference direction, deterministic exporter, palette/mask audit, isolated SDL proof | Independent `codex/art-commonwealth-platform-proof`; no gameplay source or shared-save changes |
| Gameplay owner | Active Core-food/new-game generation plan and its approved successor | Keep native station interfaces and IDs stable; art does not change A1/A2 evidence |
| CI owner | Existing compiler/build/CI maintenance | Art consumes pinned NML/Pillow tools; no workflow takeover or duplicate CI repair |
| Asset preparation, under visual owner | Train multi-angle templates, terrain slope inventory, icon inventory and provenance lists | Research/source templates only until the first family passes human review |

One visual owner approves material, scale, geometry and source conventions.
Artists can prepare independent sources against the frozen interfaces; only one
integration branch packages a family at a time. Do not reuse GRF IDs, replace
published content hashes, edit another owner's worktree or load modified content
into an archived save. Conflicting native replacement IDs require an explicit
load-order decision. A future vehicle graphics change must preserve every existing
vehicle ID and gameplay property and address this fork's exact content admission.

## Review and next gate

Build the pinned pack and current engine, then run:

```sh
python3 scripts/build_commonwealth_platform_grf.py --verify
python3 scripts/test_commonwealth_platform_art.py --output build/platform-art-proof
python3 scripts/preview_commonwealth_platform_art.py
```

The native WP11 scene is authored with capital and infrastructure. It is a
bounded art fixture, separate from the ordinary-start A1 proof. The paired test
uses unchanged original industry/rail packs, confirms static admission before
generation, compares cargo/cash/date and bounded route operation, then cold loads
and continues with and without the optional art. SDL/Xvfb captures are rendering
evidence. Paused small-scene frame/RSS measurements are diagnostic; they cannot
establish large-network performance or desktop human acceptance.

Human before/after gate: inspect both axes and mixed shelter layouts at normal
and 2x zoom, with moving trains, company colours and station transparency.
Confirm no seams, misplaced pixels, hidden train wheels, surprising occlusion or
lost rail/signal contrast. Save, quit and reload. Record the exact build, base
graphics, art hash, display/UI scale and observed frame/RSS impact. Reject or
revise the family if any of those regress. The earlier research targets of 10%
p95 frame-time and 32 MiB RSS are provisional budgets, not established results.

After this gate, the recommended next family is one freight locomotive and one
wagon on the same route, including all native directions and company recolours.
Terrain follows with one bounded slope/shore family; industries and UI follow
their own scope and compatibility review. Broader rollout requires approval of
the proof's appearance and pipeline; no simultaneous wholesale replacement.

Official technical references: [replacement sprites](https://newgrf-specs.tt-wiki.net/wiki/NML:Replace_TTD_sprites),
[sprite geometry and masks](https://newgrf-specs.tt-wiki.net/wiki/NML:Realsprites),
[zoom variants](https://newgrf-specs.tt-wiki.net/wiki/NML:Alternative_sprites).
Production rendering reads `src/table/station_land.h`, `src/sprite.cpp` and
`src/viewport.cpp`; engine capability does not imply this fork has already
authored those assets.
