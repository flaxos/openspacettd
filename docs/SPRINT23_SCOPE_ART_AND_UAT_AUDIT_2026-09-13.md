# OpenSpaceTTD Sprint 23 — Scope, Art Direction and UAT Audit

Status: **COMPLETE (documentation and delivery specification)**  
Date: **2026-09-13**

## Goal

Turn the implemented federation foundation into a decision-complete delivery sequence for a visually distinctive, fully surfaced and testable OpenSpaceTTD release. Sprint 23 reconciles the repository state through Sprint 22 and defines the work for alien world art, player rail blueprints, CST prefab rail blocks, UI completion and a new UAT demo.

## Audited baseline

Repository inspection confirms that Sprints 18–22 delivered native megacity, corridor and universe-directory windows; cluster orchestration; spaceport and Edge Conduit federation bridges; gateway telemetry/navigation; and round-trip consist order restoration. These systems have source and automated test coverage, but the existing roadmap still labels some of them as future work.

The current configured build registers 207 CTest cases. Sprint 23 uses that count as an inventory, not a fresh pass claim; release validation must run the suite at the relevant implementation sprint.

The following work remains open:

- There is no in-tree blueprint model, persistence format, capture/placement command path or blueprint library UI.
- There is no packaged CST prefab library.
- Commonwealth string rebrands exist, but the proposed in-tree industry, rail, terrain, flora, portal and arcology art packs are not present.
- Existing world biomes classify and generate regions, but do not yet provide the strong alien visual identity required for the playable release.
- Feature UI coverage is uneven: primary federation monitoring has native windows, while some operating and recovery workflows still depend on contextual views or console/daemon tooling.
- `demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav` predates later portal terminal, federation, telemetry and order-restoration work and cannot be the final all-feature UAT artifact.

## Locked product decisions

- The first visual milestone is a playable overhaul of the three UAT worlds: Temperate Core, industrial Arid Desert and Sub-Arctic Frontier. The other registered biomes retain documented target identities and follow after the showcase set.
- Blueprint v1 covers rail, signals, depots and rail stations. Portal approach track may be captured; live gates and federation identities are connection targets rather than copied objects.
- CST blocks use the player blueprint engine and ship as read-only built-ins. The initial library contains a double-track mainline, crossover, passing loop, T-junction, four-way junction, terminus, through station and portal staging yard.
- The UAT release consists of a guided solo save and a reproducible three-server federation kit.
- A feature is not release-complete until its player operation and state are accessible through UI, or its administrator-only status is explicitly documented with an operator interface.

## Delivery roadmap

| Sprint | Outcome | Required exit evidence |
|---|---|---|
| 24 | Playable alien worlds | Three visually distinct UAT biomes, readable rail infrastructure, asset provenance and save/load validation. |
| 25 | Player rail blueprints | Capture, manage, export/import, preview, transform and place rail layouts through deterministic commands and native UI. |
| 26 | CST prefab rail blocks | Eight validated built-ins with traffic-side variants, footprints, train-length guidance and operating notes. |
| 27 | Complete feature UI | All coverage-matrix gaps closed and player-facing errors actionable. |
| 28 | Guided solo UAT save | Regenerable versioned save with Story Book instructions, persistent goals and completion evidence. |
| 29 | Federation UAT and release acceptance | Three-server kit verifies live round trips, recovery, telemetry, identity, orders and cargo conservation. |

## Acceptance

- [x] Current implementation status is separated from planned work.
- [x] Art direction defines palette, shapes, materials, readability and phase/biome composition.
- [x] Blueprint and CST prefab v1 boundaries are fixed.
- [x] Every OpenSpaceTTD feature family is mapped to UI and UAT coverage.
- [x] Sprints 24–29 have dependencies and measurable exit conditions.
- [x] The legacy demo is identified as a historical fixture rather than final UAT evidence.

The detailed visual specification is in [ALIEN_WORLD_ART_DIRECTION.md](ALIEN_WORLD_ART_DIRECTION.md). The operational audit is in [FEATURE_UI_UAT_COVERAGE.md](FEATURE_UI_UAT_COVERAGE.md).
