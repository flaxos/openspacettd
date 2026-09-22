# OpenSpaceTTD Documentation Index

Start recovery work with the [master recovery plan](RECOVERY_PLAN_2026-09-15.md).
It links the baseline/evidence, defect register, updated existing UAT, AI research
gates, provenance and title milestones. [Checkpoint](../DEVBOX_HANDOFF.md).

Current acceptance entry point: [v1.1 checklist through Sprint 42](../demo/ALL-FEATURES-UAT.md),
[results sheet](../demo/UAT-RESULTS.md) and [coverage matrix](FEATURE_UI_UAT_COVERAGE.md).
See the [commit review and verification record](UAT_V1_1_REVIEW_2026-09-15.md).
Historical sprint completion is not human acceptance. Current blockers include
full content activation, player production integration and bespoke art.

Start with [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md). It is the authoritative source for current completion, evidence boundaries and planned work.

## Canonical current documents

- [Project status and roadmap](PROJECT_STATUS_AND_ROADMAP.md) — completed spikes and sprints, incomplete goals and planned Sprints 51–52.
- [Sprint ledger and index](SPRINT_LEDGER.md) — comprehensive 52-sprint index with exact commits, test evidence, UAT status, and strict status classifications.
- [Current architecture](CURRENT_ARCHITECTURE.md) — source-verified system-level architecture snapshot at HEAD.
- [Known limitations](KNOWN_LIMITATIONS.md) — documented gaps, architectural constraints and unproven claims.
- [Source-code architecture map](ARCHITECTURE_NOTES.md) — OpenTTD subsystem mapping for developers.
- [Game design and technical plan](GAME_DESIGN_AND_TECHNICAL_PLAN.md) — stable gameplay vision and architecture.
- [Feature, UI and UAT coverage](FEATURE_UI_UAT_COVERAGE.md) — feature-by-feature implementation and acceptance evidence.
- [Commonwealth world graph and expansion design](COMMONWEALTH_WORLD_GRAPH_AND_EXPANSION_DESIGN.md) — 3-tier hybrid universe topology.
- [Commonwealth lore and asset alignment](COMMONWEALTH_LORE_AND_ASSET_ALIGNMENT.md) — naming and content direction, including the incomplete asset pack.
- [Mid-to-late game corporate & tech tree spike](MID_TO_LATE_GAME_CORPORATE_TECH_SPIKE.md) — Corporate HQ on Phase 1 Core worlds, R&D Tech Tree, and Factorio/CoI-style in-kind material fabrication.
- [Alien world art direction](ALIEN_WORLD_ART_DIRECTION.md) — target visual language and the boundary between procedural styling and bespoke art.

## Sprint evidence

Dedicated sprint records are point-in-time evidence. They retain the test totals and claims made at sprint completion; they are not cumulative current-status pages. Dedicated records exist for Sprints 11–17, 23–35, 37 and 39–42; Sprint 36 is represented by UAT artifacts and Sprint 38 art remains planned. Sprints 43–48 are described in the [post-recovery roadmap](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md). Sprints 49–52 are described in the [Commonwealth expansion plan](SPRINTS_49_52_COMMONWEALTH_EXPANSION.md). Sprints 1–10 and 18–22 are covered by repository history, tests and grouped design/UAT documentation.

## UAT documentation

- [Solo UAT overview](../demo/README.md) and [Sprint 28 walkthrough](../demo/SPRINT28-UAT.md) cover the v0.4 single-player fixture through Sprint 27.
- [Federation UAT](../demo/FEDERATION-UAT.md) covers authority API and cluster-supervisor procedures. Consult the canonical status page for the live game-process limitation.
- Earlier portal, Sprint 10 and Sprint 11 guides are historical regression fixtures.

## Historical and upstream references

`DEVBOX_HANDOFF.md` is a point-in-time audit checkpoint from 2026-09-15/16 and does not reflect current merge status. `PORTAL_WORMHOLE_SPIKE.md`, `SPRINT23_SCOPE_ART_AND_UAT_AUDIT_2026-09-13.md` and superseded roadmap sections record earlier decisions. `RECOVERY_PLAN_2026-09-15.md` and `CRITICAL_BUG_REVIEW_2026-09-15.md` are recovery tracking documents; open items are reflected in the canonical status page. Generic OpenTTD documents in this directory remain upstream technical references and do not state OpenSpaceTTD feature status.
