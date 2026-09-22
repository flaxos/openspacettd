# OpenSpaceTTD Known Limitations

**Date:** 2026-09-22
**Verified at:** HEAD `a6cf83e6a6`

This document records known gaps, architectural limitations, and unproven claims.
It is derived from source audit, git verification, and comparison of documentation
against actual implementation.

---

## 1. Human Acceptance Gaps

These features have passing automated tests but **no recorded human acceptance evidence**.

| Feature Area | Sprints | Automated Tests | Human UAT |
|---|---|---|---|
| Empire Facility Overlays | 43 | ✅ Pass | ❌ Not run |
| Lore AI Competitors | 44 | ✅ Pass | ❌ Not run |
| Visual Overhaul Pack | 45 | ✅ Pass | ❌ Not run |
| Multi-Server Federation Cluster | 46 | ✅ Pass | ❌ Not run |
| Colonial Megaprojects & Alliances | 47 | ✅ Pass | ❌ Not run |
| Narrative Generator & Balancing Critic | 48 | ✅ Pass | ❌ Not run |
| Commonwealth Graph Engine | 49 | ✅ Pass | ❌ Not run |
| Gateway Staging & Charters | 50 | ✅ Pass | ❌ Not run |
| All-Feature Guided Solo UAT (v1.1) | 36 | ✅ Artifact | ❌ Not run |
| Blueprint Placement (post WP-01 fix) | 25 | ✅ Pass | ⚠️ Graphical retest pending |
| Cross-process federation transport | 35 | ✅ Protocol | ❌ Natural gate-entry not proven |

**The v1.1 UAT save (`demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav`) has never been
played through by a human tester.**

See `docs/FEATURE_UI_UAT_COVERAGE.md` for the full feature→UAT mapping.

---

## 2. Architectural Limitations

### 2.1 Single-map world capacity ceiling

All worlds share a single 4096×4096 tile map. Maximum practical capacity is
approximately 6–8 concurrent world regions. The 108-world universe graph exists
as **metadata** for trade simulation, not as simultaneously loaded map regions.

**Impact:** No path to 108 simultaneous playable worlds without a fundamental
engine redesign. This is accepted as designed for the foreseeable future.

### 2.2 Portal single-hop vs multi-hop coverage

YAPF accounts for portal shortcuts and multi-portal routes. However:
- End-to-end multi-hop routing with branching topologies has limited test coverage
- Portal save/load during mid-transit has limited black-box test coverage
- Congestion-based economic consequences are not proven in gameplay

### 2.3 Economy integration layering

Five economy systems operate as loosely-coupled overlays:
1. Standard OpenTTD industry production/revenue
2. `ProductionChainManager` monthly conversion
3. `MegacityManager` demand quotas
4. `PrebuiltTradeManager` off-world tariffs
5. `EdgeConduit` spaceport revenue

**Impact:** A coherent world economy where inter-world transport is mechanically
necessary for player success is a stated design goal but not yet proven by gameplay.
The systems coexist without a unified economic pressure model.

### 2.4 Federation is infrastructure, not gameplay

The federation protocol, identity system, transfer journal, and cluster testbed
are implemented. However:
- Cross-server transit requires manual `federation_dispatch` command
- Natural gate-entry departure between independent game processes is not proven
- No player-facing federation gameplay exists (no automatic gate-based transit,
  no multiplayer federation session)
- The cluster testbed verifies protocol correctness, not player experience

### 2.5 Blueprint system regression recovery

Blueprint placement (Sprint 25) had a user-reported crash. WP-01 repaired the
crash path. The blueprint system now:
- ✅ Passes 78 automated tests
- ⚠️ Graphical capture workflow acceptance is pending
- ⚠️ CST prefab functional routing (WP-07) is partially implemented

### 2.6 Commonwealth content activation gap

Sprint 37 NML packs exist in-tree (`pkg/commonwealth_industry/`,
`pkg/commonwealth_rail/`). However:
- The migrated v1.1 save does not activate the NewGRF packs
- Content requires manual NewGRF configuration or a fresh save with proper setup
- Engine identity fix exists but in-save activation is unproven

---

## 3. Documentation Contradictions (Resolved)

The following contradictions existed prior to this documentation audit and are
corrected by the current documentation update:

| Contradiction | Source A | Source B | Resolution |
|---|---|---|---|
| WP-01–09 merge status | `DEVBOX_HANDOFF.md` ("uncommitted") | `PROJECT_STATUS_AND_ROADMAP.md` ("merged") | **Verified merged** via `git merge-base`: `fix/recovery-wp01-wp09` IS on `main` (PR #6) |
| WP-11 merge status | `DEVBOX_HANDOFF.md` ("uncommitted") | `PROJECT_STATUS_AND_ROADMAP.md` ("merged") | **Verified merged** via `git merge-base`: `fix/wp11-content-economic-slice` IS on `main` (PR #9) |
| Sprint 49–50 completion | `SPRINTS_49_52` ("COMPLETE") | Git history | **Not merged into main**. On feature branches at HEAD `a6cf83e6a6` |
| WP-F1 gate classification | Branch tip has extra commits | PR #12 merged earlier commits | **Content partially merged**: PR #12 content is on `main`; local branch has 1 additional commit not on `main` |
| WP-F2 branch tip | Local tip at `80d2969a78` | Multiple PRs #14–#20 from this branch | **Content merged via PRs**: federation custody work from WP-F2 is on `main` through PRs #14–#20, but the branch tip itself is ahead of `main` |

### DEVBOX_HANDOFF.md resolution

`DEVBOX_HANDOFF.md` was a point-in-time snapshot from 2026-09-15/16. At that time,
WP-01–09 and WP-11 were indeed uncommitted. They were subsequently pushed and
merged. The handoff document is now historical; it should not be consulted for
current merge status.

---

## 4. Missing or Planned Work

| Item | Sprint | Status | Notes |
|---|---|---|---|
| Bespoke original art | 38 | **Planned** | No original art assets created; current visuals use palette recolouring and procedural styling |
| Expeditionary Survey Logistics | 51 | **Planned** | No source exists |
| Galactic Commonwealth Hegemony | 52 | **Planned** | No source exists |
| Space combat / planetary defence | — | **Vision** | Unscheduled idea |
| Procedural alien languages | — | **Vision** | Unscheduled idea |
| Dynamic climate change | — | **Vision** | Unscheduled idea |
| Full multiplayer federation session | — | **Gap** | No player-facing federation gameplay demonstrated |
| Natural gate-entry federation transit | — | **Gap** | Manual dispatch only |

---

## 5. Test Coverage Gaps

| Area | Automated | Black-box/Integration |
|---|---|---|
| Portal mid-transit save/load | ✅ Unit tests | ⚠️ Limited |
| Multi-hop portal routing | ✅ YAPF unit tests | ⚠️ Limited topology coverage |
| Cross-process federation | ✅ Protocol tests | ❌ No natural gameplay test |
| Commonwealth NewGRF activation | ✅ Pack manager tests | ❌ No in-save activation test |
| Production chain end-to-end | ✅ Monthly conversion tests | ⚠️ Human chain acceptance pending |
| Blueprint capture workflow | ✅ JSON/placement tests | ❌ GUI capture not tested |

---

## 6. Branching Reality

As of 2026-09-22, the actual Git state is:

| Branch | Merged to main? | Content |
|---|---|---|
| `fix/recovery-wp01-wp09` | ✅ Yes (PR #6) | WP-01 through WP-09 recovery fixes |
| `fix/wp11-content-economic-slice` | ✅ Yes (PR #9) | WP-11 economic vertical slice |
| `fix/wp10-honest-conduit-delivery` | ✅ Yes (PR #8) | WP-10 edge conduit fix |
| `fix/wp-f1-doxygen-cleanup` | ✅ Yes (PR #13) | WP-F1 documentation |
| `feature/sprint-43-empire-facilities-overlays` | ✅ Yes (PR #22) | Sprint 43 |
| `feature/sprint-44-lore-ai-competitors` | ✅ Yes (PR #23) | Sprint 44 |
| `feature/sprint-45-visual-overhaul-pack` | ✅ Yes (PR #24) | Sprint 45 |
| `feature/horizon-a-cluster-testbed` | ✅ Yes (PR #21) | Sprints 46–48 cluster infrastructure |
| `plan/wp-f2-durable-federation-custody` | ✅ Content via PRs #14–#20 | Sprints 46–48 source |
| `feature/sprint-49-commonwealth-graph-engine` | ❌ No | Sprint 49 |
| HEAD (sprint-50) | ❌ No | Sprint 50 |
| `fix/wp-f1-gate-classification` | ⚠️ Partial | PR #12 merged; 1 extra local commit |


## 2026-09-22 — CST prefab purchase-mode UAT blocker

Player-reported missing-material rejection exposed a purchase-mode discovery gap:
the ordinary company HQ is separate from corporate management, whose controls
also overflowed a single row. Blueprint Library now has a company purchase-mode
switch; HQ tabs/actions are split into rows with a dedicated mode switch. Both
use the existing authoritative command, without requiring an HQ or changing saved
mode automatically. Cash mode needs no stockpile; fabrication still requires
local-world materials. The library control supports the UAT's 640-pixel width;
the detailed HQ dashboard still requires a wider screen.

Automated verification: incremental build passed; 425 unit cases / 66,224
assertions and 436/436 CTests passed, including the new 56-assertion GUI
placement regression. File-description and unused-string linters and
`git diff --check` passed. Human retest pending.
See [UAT evidence and retest steps](../demo/UAT-RESULTS.md#2026-09-22--cst-prefab-purchase-mode-uat-blocker).


## 2026-09-22 — UAT text readability

Blueprint details and status messages now wrap; related counts share a compact
line, with more room for descriptions. Corporate HQ Overview and Fabrication
use short, wrapped instructions and a compact materials list. Repeated discount
claims and nonessential explanatory text were removed. HQ status messages wrap.
This is a presentation-only change; construction and resource rules are unchanged.

Verification: incremental build, 425 unit cases / 66,224 assertions, 436/436
CTests, both repository linters and `git diff --check` passed. Human visual retest
pending: reopen Blueprint Library and HQ Overview/Fabrication, check readability
at the user's font/UI scale, and resize the windows. Very long imported text still
requires sufficient panel height; the wider HQ dashboard is not redesigned here.

## Connected economy UAT v2.0 — 2026-09-23

The new [playable connected save](../demo/CONNECTED-ECONOMY-UAT.md) has automated
proof of repeated logistics, a 24-month soak, real city growth, reload conservation,
food starvation/recovery, CST prefab material use and research consumption.
**Human visual UAT remains pending.** The computer-use CLI was unavailable
(`orca-ide: command not found`), so no graphical acceptance is claimed.

The fixture uses 100 million startup credits, three prerequisite technologies,
dedicated service corridors, and disabled breakdowns/disasters. Cargo stocks warm
from zero through normal production and transport. Stockpiles are pooled per company
and logical world. This proves the bounded showcase, not competitive balance or
indefinite growth. Monthly supply fluctuates; rebuilding can temporarily lower town
population. All worlds remain regions of one map. The save requires the matching
v3 NewGRFs and current engine; old v2 binaries and UAT saves remain available.
See [detailed boundaries and evidence](CONNECTED_ECONOMY_UAT.md).

## Connected UAT terrain and text follow-up (2026-09-23)

The first connected-demo human run exposed 742 malformed slopes and missing
quantity/unit descriptions for all 13 custom cargos. These are fixed and the
reported crash save is recovered with full-map viewport and two-language audits.
See [recovery evidence](../demo/CONNECTED-UAT-RECOVERY.evidence.json).

Automatic terrain recovery is intentionally limited to the original marked
1024×1024, four-world connected fixture. A changed corner shared by infrastructure
causes a clear load error instead of modifying that infrastructure. Renamed or
structurally modified unrelated fixtures are not general-purpose terrain-repair
targets. Original v2/v3 content hashes remain available. This repair does not
change the single-map architecture or establish human visual acceptance; the
user must still retest panning and the affected station/industry windows.

## 2026-09-23 — Savegame authoring workflow

[Savegame authoring](SAVEGAME_AUTHORING.md) now records the reproducible build,
early terrain/text audits, logistics proof, checkpoint use, content compatibility
and publication workflow. `AGENTS.md` requires future scenario work to use it.
Documentation-only follow-up: no engine, content or save changes; human visual
acceptance remains pending. The guide explicitly distinguishes fixture-specific
assumptions from reusable practices.
