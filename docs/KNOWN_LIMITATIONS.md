# OpenSpaceTTD Known Limitations

## First Sustainable Freight boundaries — 30 September 2026

[A1 evidence and human mission](audit/2026-09-30/first-freight/README.md) cover only
the exact canonical profile and sampled native seeds. Graphical New Game discovery,
manual construction and controls remain Pending. The bounded route planner is a
proof aid, not a globally cheapest route finder or all-seed reachability guarantee.
Output-to-hub transport is affordable buffer relief and storage; it is not final
steel/food consumption or research/earned A2 access. Indefinite demand is unproven.

The native CST starter train presently incurs zero running cost because railv3
omits `running_cost_base`; output locomotives, interest, tolls and ordinary other
charges still apply. A positive CST running-cost balance would need separate owner
approval and fresh profitability evidence. No such policy change is part of A1.
The one-wagon seed101 attempt fails combined post-reload finances; it is retained,
not hidden by additional cash. A normally paid larger consist is being tested.

**Coverage correction:** the historical28September reports labelled11/101/2026
all used native seed11 due to the shared runner's hardcoded CLI argument. Those
original files remain retained; they do not establish representative seed coverage.
Current actual-seed manifests supersede that claim, rather than rewriting history.

## Organic UAT crash follow-up — 30 September 2026

The earlier headless verification omitted native foundation queries: it accepted
sloped terminal switches that crashed rendering. The new complete geometry audit
and SDL smoke check close that specific gap. Human graphical acceptance remains
Pending; the smoke test uses SDL's dummy video driver, not the user's NVIDIA
OpenGL desktop. See [evidence](audit/2026-09-30/organic-uat-crash/README.md).

Automatic recovery is restricted to the recognized 256×256, four-world legacy
UAT. It refuses to modify edited terminal infrastructure, other structures sharing
changed corners, or terrain supporting a live vehicle. Other saves are untouched;
this is not a generic repair for arbitrary corrupt terrain. Existing generated
corridor reachability, freight profitability and organic startup balance still
need gameplay acceptance independently of rendering safety.

## Tooltip crash follow-up — 29 September 2026

The reported zero-width tooltip assertion is repaired and covered by native
window tests at 100%, 150% and 200% scale. Human hover/right-click retesting of
the reported desktop layout remains Pending. This repair does not establish
acceptance of unrelated scenario-generator work or the full economy/star-map UI.
[Scope and evidence](audit/2026-09-29/tooltip-crash/README.md).

## Organic UAT World Layouts & Prefab Scenario Synthesis — 29 September 2026

- **Corridor Grading Box:** The procedural organic terrain generator flattens a central
  box of $[-8, 8] \times [-8, 8]$ tiles around world centers to height 0. This ensures
  that 2-track terminal platforms, station throat scissors crossovers, holding loops,
  and nearby canonical industries (e.g. Iron Mine, Steel Mill, Factory) sit on coplanar
  ground without triggering steep slope or tunnel errors during placement.
- **Slope Invariant Enforcement:** Non-corridor terrain undergoes an iterative neighbor
  relaxation pass that clamps elevation deltas to $|\Delta h| \le 1$. Slopes adhere to
  OpenTTD height-field invariants, avoiding sheer drops or corner mismatches.
- **Dedicated Server Town Founding:** Procedurally founded towns now explicitly initialize
  `InitializeBuildingCounts()` before house expansion or placement is queried, preventing
  null pointer dereferences in `TryBuildTownHouse`.
- **Human Acceptance Pending:** Visual evaluation of landscape naturalness, urban layout
  readability, and railway throat operation is pending human visual playthrough (Track 9).

## Integrated economy acceptance boundaries — 28 September 2026

The implementation and reproducible evidence are recorded in the
[integrated economy audit](audit/2026-09-28/integrated-economy/README.md).

- Human Fund Industry, Facilities, HQ, city, warehouse and star-map readability
  at the player's language/UI scale is **Pending**. Several custom panel strings
  remain English literals. Headless native tests do not prove visual usability.
- New games only: integrated industry content is version 5 with minimum compatible
  version 5. Never replace the legacy industry GRF in an existing save. Published
  legacy files remain unchanged and two representative saves pass native reload
  and continued simulation with their original content.
- The all-chain fixture authors its initial network and capital, then earns money
  and moves physical cargo without recurring injections. It proves reachable
  progression and profitable operation, not minimum-loan bootstrap balance. Its
  commissioned gate adds a link to an already accessible fixture world; separate
  stellar tests cover opening access and colonisation rules.
- Seven-world generation on seeds 11, 101 and 2026 verifies accessible food/stone/iron/oil,
  surveyable copper/silica and rare-mineral expansion, but exhaustive generation
  across all seeds, climates and map sizes is not claimed. Site placement retains
  bounded terrain/biome searches. Economic pacing and raw throughput need playtesting.
- Physical counters distinguish core freight, factory, city, material and explicit
  discard events. The acceptance reconciles those bounded flows; it is not an audit
  of every arbitrary crash/demolition or legacy edge-conduit/spaceport scenario.
- Federation requires matching content/rules, registration and company mapping.
  One confirmed research home is fixed; no relocation, independent save rollback,
  cross-host acquisition workflow or automatic server provisioning is supplied.
  Recovery uses coordinated checkpoints. Large-universe scale is not established.
- Commodity markets, new narratives, per-warehouse inventories and save conversion
  remain outside scope. Earlier dated limits below retain their historical context.

## Current CST expansion limits — 28 September 2026

The new implementation on `codex/cst-star-map-expansion` is based on main
`8464e9374a`. Automated evidence and exact checks are retained in the
[stellar-network audit](audit/2026-09-28/stellar-network/README.md).

- Human star-map layout, equipment logistics and cross-host Visit acceptance are
  **Pending**. English custom panels contain literal text; localization and
  nondefault UI-scale readability are not proven by native button tests.
- Each server still simulates one contiguous map with immutable logical regions.
  The lore catalogue is metadata; the new preset instantiates seven selected
  worlds. It does not load arbitrary catalogue worlds or resize existing saves.
- Hosts need matching content, disjoint generated world IDs, registration secrets
  and explicit global-company mappings before play. Native server/company
  passwords still apply. Treasury balances remain host-local.
- Remote projects resume idempotently, but recovery is accepted only from the
  latest coordinated all-host + authority checkpoint. Independent save rollback
  and arbitrary cluster power-loss recovery are not claimed.
- Committed remote projects cannot cancel and remote links cannot yet be
  demolished. Access/tolls are managed at the commissioning end. Cross-host
  acquisitions/mergers and orphan-project administration need a separate lifecycle
  workflow. Do not treat local company ownership as a claim to a whole world.
- Remote schedules support the verified station load/unload route and explicit
  gate pins. Shared order pools do not span hosts. General conditional/timetable/
  depot-refit portability is not part of this acceptance. Directory location
  updates lag train movement; a follow action may need retrying.
- Research distances and equipment quantities are initial game balance. Raw
  sites obey terrain/biome restrictions; each world is not guaranteed each
  resource. Colonisation, development and prospecting remain separate gates.
- Three-host repeated freight and reload tests do not establish large-universe
  performance, every branch topology, or full remote-project outage coverage.

See [setup and implementation boundaries](CST_STELLAR_NETWORK.md). Historical
limitations below retain their dates; the 27/28 September evidence supersedes
older blanket claims that natural scheduled federation is absent.


**Date:** 2026-09-29
**Verified at:** HEAD `bc18c2c8fc`

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
| All-Feature Guided Solo UAT (v1.1) | 36 | ✅ Artifact | ❌ Not run (superseded by v2/integrated saves) |
| Blueprint Placement (post WP-01 fix) | 25 | ✅ Pass | ⚠️ Graphical retest pending |
| Cross-process federation transport | 35 | ✅ Protocol / Cluster | ⚠️ Scheduled freight proven (PR #41); graphical multiplayer UAT pending |
| Scheduled Federation Freight & Recovery | — | ✅ Pass | ❌ Graphical retest pending (PRs #40, #41) |
| Player-Built Resource Surveys | — | ✅ Pass | ❌ Not run (PR #42) |
| CST Star Map & Commissioned Gates | — | ✅ Pass | ❌ Not run; [checklist](../demo/CST-STELLAR-UAT.md) (PR #43) |
| Unified Commonwealth Economy | — | ✅ Pass | ❌ Not run; [checklist](../demo/INTEGRATED-ECONOMY-UAT.md) (PR #44) |

**The active UAT saves (`demo/OpenSpaceTTD-Integrated-Economy-UAT.sav` and `demo/OpenSpaceTTD-Integrated-CST-Sector.sav`, superseding the legacy v1.1 save) have not been played through by a human tester.**

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

Historical pre-ruleset assessment. The integrated new-game path and its physical
custody/conversion rules supersede the claim below that integration is absent.
The current open gap is ordinary-start profitability and human usability, as
recorded in the 28 September acceptance boundaries above.

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

Historical pre-PR #40/#41 assessment. Natural scheduled cross-server freight and
coordinated recovery are implemented and automatically tested; the manual-only
claims below no longer describe current behavior. Company/project/order lifecycle,
interrupted-operation and human host-visit acceptance remain open. Independent
always-on crash recovery would require a separate architecture decision.

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

## 2026-09-25 — Federation scheduled freight (draft)

Branch `fix/federation-scheduled-recovery` integrates main with PRs #36/#37
and the existing UAT cargo/terrain recovery and authoring guide. New work adds
nonblocking multiplayer authority requests, explicit company/station mappings,
portable station schedules, packet provenance and coordinated-checkpoint tools.

This is **in progress, not sprint completion**. Combined unit tests previously
passed 438 cases / 66,610 assertions. The transport-only fixture passed a
30-second authority outage and destination restart with joined clients. The
scheduled native coal fixture completed three loaded deliveries and empty
returns (180 coal accepted) before its 600-second timeout; the required five
cycles and full recovery matrix have **not passed**. Conservation/payment checks
after five cycles, nonempty custom-state reload, blocked arrival and checkpoint
phase coverage remain unverified. Human federation UAT remains pending.

See [the implementation record](FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

## Federation freight acceptance follow-up — 27 September 2026

PR #40 is merged on main (`d886260607`). Follow-up work is on
`fix/federation-freight-recovery-acceptance`.

**Scheduled freight passes:** five automatic loaded outbound trips and empty
returns delivered 300 coal. Native production, unallocated cargo, station losses,
remaining stock and consumption reconcile exactly; all native cash changes also
reconcile. The two clients remain connected with no detected desync or duplicate.
The previous 600-second timeout was shorter than the native route's five-cycle
runtime. The harness now budgets startup plus time per requested cycle.

**All nine recovery cases pass:** physical blocked arrival, loaded-arrival
reload, authority-custody reload, prepared-departure reload, a 30-second authority
outage, destination restart, source restart, both clients reconnecting, and full
coordinated reload. Each case completes three further loaded deliveries and
empty returns with exact cargo and cash reconciliation. The retained sequence
contains 32 round trips and 1,920 coal delivered. During the authority outage,
both clients stayed connected and the maximum measured console response was
0.332 seconds; the original prepared request completed exactly once.

The loaded cargo/master-schedule save/load regression passes. Combined tests pass
439 cases / 66,636 assertions, isolated CTests pass 450/450, randomized federation
tests pass 59 cases / 1,172 assertions, and all three Python checkpoint/stdout
regressions pass. Current and legacy solo cargo/terrain recovery regressions also
pass. Graphical federation UAT remains separate and pending.

Machine evidence is recorded in `demo/FEDERATION-FREIGHT-RECOVERY.evidence.json`,
including the frozen binary hash, checkpoint hashes, accounting and continuation
provenance. Recovery uses only the latest coordinated set; arbitrary crash
recovery and old/mixed-save rollback are outside this acceptance boundary.

See [acceptance method and evidence](FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

## Player-built resource economy — 28 September 2026

Human visual UAT and starting-economy balance remain pending. Site abundance is a
bounded target; unsuitable terrain, insufficient world area and content callbacks
can prevent requested sites. Unrecognised primary resources use the basic research
tier. Third-party NewGRF coverage is not exhaustive. Survey knowledge is filtered
in the UI/API, not cryptographically hidden from clients. Resource sites do not
introduce depletion, exclusive claims, survey vehicles or existing-save conversion.
The connected resource fixture proves a funded authored network, not low-budget
startup balance or random generation. Its town-growth check is separate from the
original showcase's three-new-houses-within-ten-minutes target.
[Details and reproduction](PLAYER_BUILT_RESOURCE_ECONOMY.md).

## Agent workflow tooling — 30 September 2026

Codex CLI skill discovery is verified. Antigravity discovery and interactive
usefulness remain Pending. Instruction size reduction does not prove quota savings.
[Verification boundary](AGENT_WORKFLOW.md#delivery-record--30-september-2026).
