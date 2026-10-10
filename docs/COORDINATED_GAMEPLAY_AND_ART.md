# Coordinated gameplay and sci-fi art

## Current synchronization — 10 October 2026

The [checkpoint alignment](audit/2026-10-10/checkpoint-alignment/README.md)
supersedes historical workstation/remote availability statements below. GitHub
still has main `85c423b4`, draft PR66 `7bc29a60`, and draft PR71 `1c95f075`.
PR72 is merged into PR71, not main. The October 7 order of dependencies remains
unchanged; no copyable prompt is dispatched by this documentation update.

The gameplay helper has four changed files, including Python guard tests, and
matches the preserved October 7 post-review patch. The asset GRF matches its
retained hash. Today's Antigravity conversation runs from the primary checkout,
not either isolated helper/art tree, and finished waiting on a fresh-run proposal.
That proposal missed the retained full-basket proof. Use the existing station
adapter review as the next source task; do not replay the older source-save plan.
Keep one gameplay owner and a separate art owner. Current exact-final helper
native verification, actual town growth and gate runtime acceptance remain open.

## Dispatch update — 7 October 2026

This update supersedes the 5 October status and copyable prompts below. The
earlier sections remain as the dated planning baseline. The sole
implementation-authoritative plan is still [ACTIVE_EXECUTION_PLAN.md](ACTIVE_EXECUTION_PLAN.md);
its 3 October campaign clocks and load allowances have expired. This document
does not renew them or launch a worker. [The station adapter handoff](handoffs/CORE_TOWN_STATION_ADAPTER.md)
owns the narrower gameplay source task already underway.

### Verified position and evidence limits

| Area | Current position | Limit |
| --- | --- | --- |
| Main and stack | Last locally verified main `85c423b4`; [PR66](https://github.com/flaxos/openspacettd/pull/66) remains draft on main at `7bc29a60`; [PR71](https://github.com/flaxos/openspacettd/pull/71) remains draft on PR66 at `1c95f075`; [PR72](https://github.com/flaxos/openspacettd/pull/72) merged into PR71, not main. GitHub PR pages inspected 7 October show these states and no later PR71 commit; local GitHub API and Git fetch are unavailable, so main's current exact remote head and newly run CI cannot be certified. | The 5 October exact-head workflow results below are historical for those heads. No CI covers the current uncommitted work. Do not merge the draft stack by inference. |
| Gameplay | The preserved 5 October archive was checked by ZIP CRC and 24 checkpoint hashes. Operation 018 proves one atomic 60 FOOD + 20 STEL + 20 BALL consumption at tick 284,995, growth and passenger multipliers 1.0 and native growth hook observed. Final paused checkpoint is tick 304,384, SHA256 `89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f`. Train 28 restarted in operation 003. | `native_growth_enabled=false` at the event; population 1,191 and 29 houses did not rise. No repeated basket, sustained deliveries, safe post-helper cold continuation or human UAT is proved. The old 65,536-tick/train-28 report is not the final retained endpoint. See the handoff's 6 October evidence section. |
| Gameplay ownership | Isolated `ost-core-town-helper` branch `codex/dev-core-town-station-adapter` descends from PR71 via planning commits `4b79537` and `9b971a1`. It has uncommitted changes only in `connected_economy.{cpp,h}` and `test_core_construction_basket.cpp` for an offline quote/staged paid new Core station and FOOD order retarget. The handoff remains the source specification. | No build, native adapter test, campaign, save load, commit or PR for this diff has been verified by this planning pass. Its treatment of current pickup `OT_LOADING` requires native order-safety proof against the handoff's rejection wording. Do not dispatch another implementer onto this checkout. |
| Graphics | [PR64](https://github.com/flaxos/openspacettd/pull/64) merged four optional platform sprites with normal/2x and 8bpp fallback. Isolated `ost-gate-art` branch `codex/art-wormhole-gate-family-light` has uncommitted original 16-sprite gate sources, exporter/tests, NML, manifest and a GRF file. The manifest records a 0.9.0 compiler and a 16-sprite GRM/Action A audit; its older loader specification still says binary compilation was not done. | The current tree has no C++ gate resolver or `DrawTile_TunnelBridge` selection edit. The GRF/manifest claim needs reproducible verification; there is no in-game gate rendering or human appearance result. No phase-1 fleet sprite family has started in the inspected checkouts. |

The local planning checkout is independent of both uncommitted implementation
trees. Its Git objects were recovered from a damaged earlier planning clone;
do not repair or clean the older clone in place. Keep owner saves, running game,
Antigravity IDE, private builds, art preview and archived evidence untouched.
The recorded external Antigravity dispatches met approval blocks; these prompts
are handoffs, not permission to retry those blocked commands.

### Ordered independent sprints

| Order | Gameplay, PR71 based | Independent cosmetic art, main based |
| --- | --- | --- |
| 1 | **Core Receiver Adapter Review.** Finish the already-started native quote and paid station/link/order helper against [its handoff](handoffs/CORE_TOWN_STATION_ADAPTER.md). First resolve the `OT_LOADING` pickup question with a native InsertOrder/DeleteOrder test: unchanged current order, cargo and index, or reject that state. Check exact receiver radius/FOOD acceptance and the proposed two-tile site through native commands. Finish at a scoped reviewed commit with focused/native failure and partial-payment tests; disclose unavailable full gates. No owner save load or gameplay advance. | **Wormhole Gate Asset Checkpoint.** Preserve and review the existing uncommitted package; reproduce exporter, 8bpp/normal/2x sheets, GRF build and byte-level action/property audit using its pinned tool before adding loader code. Freeze GRM sprite allocation and reload-safe content admission. Finish at a scoped asset checkpoint with manifest/hash and source limitations; no ordinary tunnel replacement. |
| 2 | **Core Basket Repeat and Cold Proof.** Only after the adapter passes and a new single active execution handoff specifies exact checkpoint, hashes, native quote and finite load/tick/wall limits, use a copied paused checkpoint. Prove one paid FOOD delivery at the nearer actual Core receiver, another real FOOD+STEL+BALL consumption, custody/cash reconciliation and fresh-process state equality then safe continuation. Check growth hook separately from actual house/population growth. Stop at first integrity or native-command defect; retain partial paid work. Human mission: inspect Core shortage, follow FOOD unloading, observe the next monthly basket and paused cold state. | **Wormhole Gate Runtime Family.** In the existing art branch after asset review, add only optional resolver and gate-only renderer selection needed to display the compact linked cyan/unlinked amber family in all four directions. Ordinary rail/road tunnels stay native. Finish with native normal/2x/8bpp captures, occlusion, overlays, train entry/exit, matched deterministic movement/cargo/money/orders/registry, reload and content admission. Human normal-zoom review remains separate. |
| 3 | **Player-Funded Electronics Loop, queued.** Quote one legitimate Materials II and physical prosperity-cargo chain only after basket closure; freeze exact existing recipe/sites and a replacement active plan. Normal paid research/producer/delivery and cold proof are the finish. Missing resource or changed phase/role rules return a product decision. | **Phase-1 Fleet Family, queued.** Catalog actual phase-1 purchase/services first, then reskin one existing locomotive and one wagon using original industrial sci-fi art, all supported directions/load/menu/depot views, normal/2x/8bpp and company colour. Preserve rail v3 bytes and every vehicle ID, property, callback and availability rule; use a separate version/fixture if optional graphics-only admission is unavailable. Finish with property equivalence, moving train/gate/platform captures, deterministic state and cold proof; list uncovered units. |
| 4 | No automatic next gameplay slice. Reassess the canonical roadmap from delivered evidence. | **Combined Visual Acceptance.** Only after both families are reviewed, the integration owner assembles a disposable fixture with accepted platform art and pins its content manifest. Check colour, occlusion, load order, comparable frame-time/RSS, save/reload and one human five-minute freight mission. This is initial family acceptance, not a complete reskin. |

Ownership stays disjoint: gameplay edits the connected-economy adapter/tests;
gate art owns `pkg/commonwealth_gates/`, its exporter and a narrow renderer/
loader hook; fleet owns a separate graphics-only package. One integration owner
serializes shared CMake/manifest and canonical status changes. Art must not
change `PortalRegistry`, `UniverseGraphManager`, YAPF, commands, save schema,
ticks, RNG, movement, resource economy or phase/role. Gameplay must not change
renderer or published art packs. The Federation reliability track remains
parallel and outside these edits. Offline unlimited development money is
approved for functional proof only; physical resources, receipts and custody
remain real, with profitability and broad balance deferred.

### Current copyable Antigravity handoffs

Send a prompt only to an assigned, authorized session after checking its exact
workspace and ownership. The prior 5 October prompts below are superseded.

**Gameplay adapter — existing owner/review only:**

```text
OpenSpaceTTD: resume only the existing Core Receiver Adapter Review on branch
codex/dev-core-town-station-adapter, based through 9b971a1 on PR71 1c95f075.
Read AGENTS.md, ost-dev/uat/deliver, docs/ACTIVE_EXECUTION_PLAN.md and
docs/handoffs/CORE_TOWN_STATION_ADAPTER.md including the 6 October evidence.
Inspect the current uncommitted connected_economy and basket-test diff before
editing; do not duplicate or reset it. Prove native order safety for train 4's
current pickup OT_LOADING at order index 0 before admitting it; reject if proof
fails. Complete native quote, staged paid station/link/order and partial-result
tests within the handoff's one site and command bounds. Run focused and delivery
checks feasible in this private build; report exact passes and NOT RUN gates.
No owner save load/advance, live game, art files, new fleet, economy/growth rule,
campaign authority, push, PR or merge. Return scoped diff/commit and blockers.
```

**Gameplay proof — after adapter delivery and a new active handoff:**

```text
OpenSpaceTTD: execute only Core Basket Repeat and Cold Proof as frozen in the
new docs/ACTIVE_EXECUTION_PLAN.md; use docs/COORDINATED_GAMEPLAY_AND_ART.md and
the delivered station-adapter commit. Read AGENTS.md and ost-dev/uat/deliver.
Verify checkpoint SHA256, source/binary/content and all old usage before the
first load. Use offline infinite_money only as recorded, normal native commands
and a copied save. Prove actual paid FOOD arrival, a second full monthly basket,
cash/cargo custody and fresh-process equality; distinguish growth hook from
house growth. Stop on the first hard defect or finite bound, keep paid partial
evidence and report human UAT separately. No reset of expired 3 October clocks,
art edits, finance balancing, new map/economy design, merge or next slice.
```

**Gate runtime — after existing asset checkpoint review:**

```text
OpenSpaceTTD: continue the existing codex/art-wormhole-gate-family-light branch
only, based on verified main 85c423b4 unless live main requires a documented
rebase. Read AGENTS.md, ost-dev/uat/deliver, the gate asset README/loader spec
and docs/COORDINATED_GAMEPLAY_AND_ART.md. Preserve its uncommitted 16-sprite
source/GRF; first reproduce exporter/GRF/action audit and reconcile stale
documentation. Implement a reload-safe optional sprite resolver and a gate-only
DrawTile_TunnelBridge hook. Keep ordinary tunnels native and all routing,
commands, save, tick, RNG, cargo and train behavior unchanged. Validate four
directions, linked/unlinked, 8bpp/normal/2x, overlays/occlusion, native movement
and cold content compatibility. Return a scoped commit, exact checks and human
appearance Pending. Do not touch gameplay branch, fleet properties or live game;
no push, PR or merge from this handoff.
```

**Fleet first family — after gate acceptance and catalog freeze:**

```text
OpenSpaceTTD: implement only Phase-1 Fleet Family in
docs/COORDINATED_GAMEPLAY_AND_ART.md from a separate verified-main art branch.
Read AGENTS.md and ost-dev/uat/deliver. Catalog actual phase-1 purchasable
engines/services, choose one existing locomotive and one wagon, and create
original Commonwealth industrial sci-fi sprites for supported directions,
load, menu and depot views in normal/2x/8bpp and company colours. Preserve all
vehicle IDs, physics, costs, capacity, callbacks, refits, tech/year and economic
roles; preserve published rail v3 bytes. Prove graphic/property equivalence,
moving train and cold fixture before a human appearance review. Record units
not yet covered. Do not edit gate/gameplay branches or claim a complete reskin.
Return a scoped commit/checks, no push, PR or merge from this handoff.
```

Stop and replan at the actual product forks listed at the end of this document,
including independent maps/newglobalmap, role/phase or train gameplay changes,
new save/replicated command contracts, broad economy change, or art admission
that affects simulation. A failed native site/order proof is a bounded adapter
blocker to report with evidence, not a reason to change those contracts.

Planning snapshot: 5 October 2026. This is the detailed specification linked by
[the canonical roadmap](PROJECT_STATUS_AND_ROADMAP.md). It is a dispatch aid,
not a second active execution plan. Only [ACTIVE_EXECUTION_PLAN.md](ACTIVE_EXECUTION_PLAN.md)
authorizes the current implementation; an expired load/clock is never renewed
by this document. No worker is launched by this planning delivery.

## Current evidence

| Item | Verified ref/state | Evidence boundary |
| --- | --- | --- |
| Main | `85c423b4a2dd57e5a2c2cf1f6d29bc2e4667845d`, PR67 merged | Contains functional-first direction, generation and platform proof; excludes open PR66/71. Empty combined status response is not proof of green main CI. |
| PR64 platform art | Merged `211310a791527c44ce2aa683861fbb537bda73ad`; head `b624eadc791d5ffb046949afe334093f31dda5a4` | Six returned exact-head PR workflows succeeded. Four native platform IDs 1069–1072, 8bpp fallback and normal/2x 32bpp only. Human appearance Pending. |
| PR66 baseline | Open draft, `codex/generation-money-comparison`, head `7bc29a60cc1bc60b4d555d7500c2ded79c175eaa`, base main | Seven returned exact-head workflows succeeded, including Build and Nightly. Assisted FOOD/HQ/Materials I evidence; ordinary-start viability unproven. |
| PR71 basket | Open draft, `codex/first-core-construction-basket`, head `1c95f075c0bdd4d6ada912b58cc2993302520a94`, base PR66 branch | Seven returned exact-head workflows succeeded, including Build/Nightly. Four services built; paid IRON/native STEL conversion in stopped diagnostic evidence. Core STEL/BALL deliveries, monthly full basket and cold continuation unproved. |
| PR72 UI | Merged into PR71, not main, merge `1c95f075c0bdd4d6ada912b58cc2993302520a94`; head `cea137e1dded0d8fb888377f2184faf7625ec165` | Six returned exact-head workflows succeeded. Complete basket display/growth labels and tests; static fit estimate, rendered/human UI acceptance Pending. |
| PR68 | Open historical draft | Superseded by PR69 integrated into PR66. Do not duplicate, rewrite or close as part of planning. |

Live GitHub read-only queries and isolated Git fetch agree on these heads/bases.
Main is an ancestor of PR66; PR71 contains PR66 and PR72. Workflow results are
returned first-page PR-triggered runs, not an exhaustive platform/job audit or a
new local test run. Relevant run IDs: PR66 Build36933977264/Nightly36933977191;
PR71 Build37105921119/Nightly37105921097; PR64 Build36775415462;
PR72 Build37104573515. See the corresponding
[PR66](https://github.com/flaxos/openspacettd/pull/66),
[PR71](https://github.com/flaxos/openspacettd/pull/71),
[PR64](https://github.com/flaxos/openspacettd/pull/64), and
[PR72](https://github.com/flaxos/openspacettd/pull/72) for original scope/evidence.

Local source checkout `/home/flax/games/openspacettd` is clean at `2fbd2dfca9`
on the older `codex/reconcile-first-freight-plan`. Its historical
`DEVBOX_HANDOFF.md` explicitly delegates current authority to the roadmap/ledger;
do not replace it with another tracker. The clean existing UI worktree at
`/home/flax/Documents/Codex/2026-10-03/task-5/ost-antigravity-next` is at `cea137e1dd`.
Prunable review worktrees were preserved. Local Codex memories inventory exposed
no readable matching memory files; no undocumented memory claim is used.

Read-only host process inspection found Antigravity IDE and `agy` PID94922;
its cwd is the primary OpenSpaceTTD checkout. No `openttd` process appeared in the
name-filtered snapshot. This does not prove the agent is progressing, the game
has no differently named process, or a preview is safe to reclaim. Leave all
existing sessions alone; do not send a prompt until task ownership is established.

The preserved owner bundle `/tmp/OpenSpaceTTD-Core-Basket-Owner-UAT-2026-10-03`
records save tick189696, cash£3,339,266/debt£100,000, save SHA256
`ae2bbfeb73d24648cfc17bb529dee92b7d17e614ef2f385cd7707554df0b47e2`, source `6acb5094e9`.
Its later paid IRON→52STEL diagnostic is explicitly outside that paused save.
The supplied context of a 65,536-tick stop and train28 not restarting could not
be independently matched to a retained local manifest in this inspection; treat
it as reported context, not a new verified endpoint. Reconcile the actual latest
checkpoint before executing. Offline unlimited development money is approved;
this does not grant cargo, technology or research, erase debits, or prove balance.

## Source-grounded graphics status

`src/tunnelbridge_cmd.cpp:DrawTile_TunnelBridge` detects portal/unlinked gates
but still chooses native tunnel imagery and applies visual-state palettes.
`src/portal/visual_overhaul.cpp` has layout/profile/state helpers; its 18-tile
metadata and conceptual JPGs in `pkg/commonwealth_visuals/` do not prove those
assets render in the game. `pkg/commonwealth_rail/commonwealth_rail.nml` defines
vehicle properties/items but no authored `spriteset`/`graphics` blocks. The older
Sprint45 description of a comprehensive reskin is therefore not current visible
acceptance evidence. PR64 is a graphics pipeline proof, not a complete reskin.

`src/portal/integrated_economy.cpp:CityDemand/EvaluateCity` owns Core demand and
actual atomic STEL/BALL consumption after FOOD. The PR72 formatter exposes
shortages without replacing those rules. Phase1_Core and integrated EconomicRole
are distinct concepts; neither art track may conflate them.

## Ownership and interfaces

| Track | Owning edit set | Frozen interfaces / exclusions |
| --- | --- | --- |
| Gameplay | `scripts/test_core_construction_basket.py`, `src/portal/connected_economy.{cpp,h}`, relevant basket/economy tests; only reproduced defects in `integrated_economy.*`/`town_cmd.cpp` | No gate renderer, art source, rail pack properties or graphics; no live owner save/profile/build edits. |
| Gate cosmetics | New gate-family source/exporter/assets/manifest; narrow draw-only hook in `src/tunnelbridge_cmd.cpp` and adjacent sprite-loading declaration if required; dedicated visual regression | No PortalRegistry/UniverseGraph/YAPF, train movement, placement/commission commands, save handlers, recipe or game-tick mutation. |
| Fleet cosmetics | New fleet sources/exporter/assets/manifest; explicitly versioned graphic-only rail packaging integration after scope freeze | No vehicle properties/IDs/cargo/refits/availability/phase rules, train movement, research or economy source. |
| Integration owner | Shared CMake/package manifest/config integration and canonical status/ledger/UAT updates | One integration owner serializes shared edits; workers return scoped changes first. |

Use one isolated branch and private build/profile/output per assigned track.
Gameplay starts from PR71 exact head; gate/fleet use live main unless a named
render dependency requires a documented stack. Never import the whole gameplay
stack into an art PR to obtain a fixture. For combined review, use a disposable
integration branch after both scoped deliveries. Published GRF versions/hashes,
archived saves, station pack and running previews remain intact. Do not modify
CMake, shared manifests or canonical prose concurrently: return integration notes
and let the integration owner perform those changes once.

## Ordered gameplay sprints

### Checkpoint and Ownership Reconciliation

Player outcome: the existing material network has one trustworthy resume source
and the work belongs to one executor. This is the first bounded read-only task.

Read the current worker/session handoff and PR71 evidence; inspect exact head,
local changes, binary/content/config/save hashes and counters. Identify whether
train28 is a wrapper/start-stop issue, a real route/order fault, or simply pending
supply using retained logs/source. Do not load or advance the owner game. Resolve
the difference between the reported 65,536 ticks, bundle tick189696 and later
stop evidence. Check `c919288f18` observer correction against persisted semantics;
normalizing wagon caches must not conceal physical state differences.

Finish: a verified checkpoint manifest and one exact next action, recorded in the
existing basket audit and active plan, with expired bounds archived. If a healthy
worker already owns the same outcome, hand the findings back and queue no second
implementation. Without a safe save or identifiable state, return a bounded
rebuild proposal instead of silently choosing another campaign. No engine rebuild
or campaign needed for this read-only sprint.

### Core Basket Closure

Player outcome: real iron/steel and stone/ballast services feed the Core city,
missing BALL visibly prevents construction, then a complete basket is consumed
and growth permission survives a cold continuation.

Dispatch only after reconciliation updates the sole active plan with the actual
checkpoint and a renewed finite execution ledger. Preserve the owner-approved
unlimited offline money mode and defer profitability; resource starvation is a
real outcome. Suggested continuation envelope: one copied verified checkpoint,
one planned cold load, fixed four services/industries/town, no new FOOD fleet,
at most 360×2048 total native ticks and 75 minutes from the first advance.
Count diagnosis within that envelope; fail at integrity defects and record slow
supply honestly. These are proposed fresh limits, not resurrection of old clocks.
Normal orders/refits/start-stop are allowed; no cash/cargo/RP/tech authoring,
new research/factory/gate/warehouse or automatic strategy expansion. If renewable
funds require a policy change beyond the existing saved devmoney mode, replan.

Finish/checks: paid IRON/SILC inputs; native 2IRON→1STEL and 2SILC→2BALL;
positive paid STEL/BALL arrival at the real Core receiver; one sufficient
FOOD+STEL/missing-BALL monthly control with retained STEL and no construction;
one complete live basket at the actual evaluation; normal growth hook and 1/1
multipliers; hash-verified safe save; fresh-process paused equality followed by
new STEL/BALL delivery and a second consumed basket. Reconcile city/train/station/
industry custody, explicit losses, all receipts/debits and fixed debt. Supply a
5–15 minute observation mission; record human UAT separately and disclose waiting.
Use focused basket tests/harness, then `ost-deliver` full code gates and exact-head
CI. Do not label it ordinary-start balance or pristine fresh replay.

### Player-Funded Electronics Loop

Queued successor, not implementation authority. Player outcome: one normal
Materials II research action and a physically supplied electronics chain delivers
one existing prosperity cargo to the city. Choose the smallest legitimate chain
from current recipes/sites after basket closure, not both CHIP and CCRY by fiat.

First prepare a paid-site quote: retained checkpoint, exact II prerequisites,
resource sites, extraction/factory availability, recipe cargo labels, train
availability and receiver acceptance. Phase and role remain unchanged. If absent
copper/silica or kit/placement rules require a product decision, return that fork.
Freeze exact industries, cargo services and proof budget in a replacement active
plan before implementation. Existing gates/backbone may be reused. No newglobalmap,
sweeping economy change, virtual goods, new cargo/tech, competitors or lore scope.

Finish: normal II completion; normal surveyed/funded producer and factory commands
where existing rules require them; physical recipe inputs/output, a paid valid
city delivery, measured prosperity evaluation and cold continuation. Controls
must show unmet input/research denies without payment/mutation; repeated delivery
is measured independently of profitability. A single city reserve increase is
not prosperity consumption. Use existing economy/resource/research tests and
native harness; full code gates and identified human mission at delivery.

## Ordered cosmetic sprints

### Wormhole Gate Family

Player outcome: portal gates read unmistakably as engineered wormholes at normal
zoom, with a visible aperture/horizon and frame instead of a tunnel mouth.
Ordinary rail/road tunnels retain their current appearance.

Prepare an original Commonwealth industrial sci-fi family: graphite/steel,
pale ceramic edges, restrained cyan service light and amber standby cues. Avoid
inventing a new faction, technology, lore name or monumental footprint. Start
with one compact gate family in all four diagonal orientations, normal and 2x,
8bpp fallback, appropriate company masks only where already supported. Read
`docs/COMMONWEALTH_PLATFORM_ART_PROOF.md` for provenance/geometry lessons; extend
its pipeline approach without assuming Action A can distinguish gates from
ordinary tunnels. The portal predicate exists in the renderer: use a narrow
render-only selection hook if required. Freeze asset-loading/static admission
strategy before code; do not apply global tunnel replacement to satisfy this goal.
First prove a static horizon. Additional animation only if existing visual-state
hooks suffice without new persistent state/RNG/tick mutation. A cosmetic pulse
must not promise corrected holding/transit gameplay behavior.

Finish/checks: native captures of linked and unlinked gates, each orientation,
normal/2x and 8bpp fallback, ordinary-tunnel control, train approach/occlusion/
entry/exit, reservation overlays and electrified/non-electrified rail. Test current
holding visual state when a safe existing fixture supports it; report unavailable
states rather than forge registry state. No hidden track, signal ambiguity,
clipping or adjacent-tile sorting damage. Compare baseline/art cargo, money,
orders, portal registry and movement at matched deterministic ticks; cold reload
and content compatibility follow the actual chosen admission mode. Do not demand
byte-identical saves when content differs. Rendering must not modify simulation.
Reproducible exporter/manifest/action or property audit; retain original content
hashes. All code/content delivery gates apply. A human before/after review of this
family gates a broader graphics rollout; SDL/Xvfb proof remains separate.

### Phase-1 Fleet Family

Player outcome: trains seen serving phase-1 worlds look like the same industrial
sci-fi setting as gates. This means visual coverage for existing usable vehicles,
not an invented gameplay fleet or phase/economic-role reassignment.

Start with one existing locomotive and one wagon, chosen by a read-only catalog
of actual phase-1 services/purchase UI, engine item IDs, technology/year/cargo and
current sprite selection. Do not assume Phase1 means EconomicRole::Core or force
a maglev visual onto ordinary steam physics. A readable futuristic industrial
shell can preserve an existing drivetrain. Provide all native vehicle directions,
normal/2x and 8bpp fallback, company recolours and empty/loaded views where the
existing item supports them; menu/depot thumbnails must agree. No new IDs,
lengths, capacities, speed/power/weight/cost/running cost, callbacks, refits,
introduction dates, reliability, purchase gating or train spacing changes.

The current rail v3 exact-content dependency must remain byte-identical. Decide
whether a truly graphics-only optional layer is supported; otherwise produce a
separate explicit version with equivalent gameplay property audit and new-game
fixture, never silently substitute a pack in the owner save. If runtime content
admission requires simulation callbacks or a new compatibility contract, stop
and return the concrete integration decision. Default is the small compatible
family, not a universal/world-dependent dynamic fleet skin.

Finish/checks: one loco/wagon visible in purchase/depot and a moving service in
all directions, curves/slopes, gate entry/exit and beside accepted platforms;
company palette and load states; reproducible sources/exports; exact property
and catalog equivalence with baseline; matched native custody/payment/movement
and cold continuation in an isolated compatible fixture. Human normal-zoom
readability and appearance acceptance precedes additional fleet families. Return
an inventory of uncovered existing phase-1 units rather than claiming all trains
are reskinned. Next coverage is a named follow-up in the canonical roadmap.

### Combined Visual Acceptance

Player outcome: the accepted platform, gate and initial train family appear
coherent while a real freight service operates. Integration owner assembles only
reviewed deliveries in a disposable fixture, pins manifest and checks load order,
company colours, normal/2x, busy occlusion and baseline/art frame-time/RSS under
the same scene. Existing provisional budgets are not measured performance claims.
Run save/reload, leave optional art disable/removal claims conditional on proven
admission, and give one five-minute before/after mission. Human pass/fail applies
to this exact family/build/content/display only. This is bounded coverage, not a
terrain/industry/UI overhaul or federation certification.

## Ready-to-use Antigravity prompts

Copy only the prompt for the named approved task. The planning branch is
`codex/plan-gameplay-scifi-tracks`, based on PR71 `1c95f075c0`. Before dispatch,
transfer the reviewed planning document into the executor's isolated branch;
do not make art depend on unmerged gameplay merely to obtain this prose.

**Gameplay reconciliation (safe first task):**

```text
OpenSpaceTTD: perform only Checkpoint and Ownership Reconciliation in
 docs/COORDINATED_GAMEPLAY_AND_ART.md. Read AGENTS.md, historical DEVBOX_HANDOFF.md,
 .agents/skills/ost-plan/SKILL.md and the actual ACTIVE_EXECUTION_PLAN.md. Inspect
 Git state, source/tests and retained basket manifests first. Check the existing
 agy/Antigravity task ownership without messaging, interrupting or replacing it.
 Base source evidence on PR71 1c95f075c0bdd4d6ada912b58cc2993302520a94 and recheck
 live heads. Reconcile the reported 65,536 ticks/train28 with the retained save
 provenance; never infer the latest checkpoint from its filename. No game loads,
 advances, save changes or implementation. Return exact safe resume manifest,
 remaining defect, and bounded next handoff in the existing active-plan/audit
 records. Preserve all stopped attempts. Do not push, PR, merge or start a worker.
```

**Gameplay closure (after the reconciled active handoff is recorded):**

```text
Implement only Core Basket Closure in docs/COORDINATED_GAMEPLAY_AND_ART.md and
 the reconciled docs/ACTIVE_EXECUTION_PLAN.md. Work in your assigned isolated
 PR71-based branch; recheck owner and exact base. Use ost-dev, ost-uat and
 ost-deliver. Keep saved offline unlimited development money; prioritize real
 resource flow and defer financial balance. Preserve historical counters and
 archives. Fix only the evidenced resume/service defect; complete paid IRON/SILC,
 native steel/ballast, missing-BALL control, actual full monthly basket and cold
 continuation exactly within the new active ledger. Do not reset old deadlines
 or reuse consumed load authority. Keep art/renderer/rail packs untouched. Report
 cargo/cash provenance, exact tests/CI, partial failures and human UAT separately.
 Stop at the finish line or material replan trigger. Return a reviewed commit;
 push/PR only when separately included in this dispatch. No merge/next slice.
```

**Gate cosmetics (after distinct ownership and scope are confirmed):**

```text
OpenSpaceTTD: implement only Wormhole Gate Family from
 docs/COORDINATED_GAMEPLAY_AND_ART.md in the assigned isolated art branch from
 verified main. Read AGENTS.md, ost-dev/uat/deliver and platform-art pipeline
 documentation. Inspect actual DrawTile_TunnelBridge sprite selection first.
 Create original compact engineered wormhole art in four directions, normal/2x
 and 8bpp fallback. Use a gate-only draw hook if necessary; ordinary tunnels must
 remain unchanged. No routing, commands, placement, registry, save schema,
 movement, RNG, tick or economy changes. Freeze asset/admission strategy before
 coding; return a blocker if it needs a gameplay contract change. Own only the
 gate files/draw hook/test; return CMake/manifest integration notes to the single
 integration owner. Use private build/profile/fixtures and preserve live sessions.
 Prove rendering and baseline/art native state equivalence, cold compatibility
 and reproducibility. Report human appearance Pending with one short mission.
 Stop after scoped verified commit; no push/PR unless separately dispatched,
 no merge, fleet rollout or wholesale reskin.
```

**Phase-1 fleet (after gate-family review and selection freeze):**

```text
OpenSpaceTTD: implement only Phase-1 Fleet Family in
 docs/COORDINATED_GAMEPLAY_AND_ART.md. Use an assigned isolated main-based branch,
 private build/profile and ost-dev/uat/deliver. Catalog actual available phase-1
 units first; freeze one existing loco and one existing wagon. Produce original
 industrial sci-fi sprites in all directions, normal/2x, 8bpp and company colours,
 including supported load/menu/depot views. Keep every ID/gameplay property,
 gating rule and physical consist length unchanged. Preserve published rail v3
 bytes; prove supported graphics-only admission or use an explicit new version
 and isolated new fixture. Never substitute content in an archived owner save.
 No phase/role redesign, new economy, vehicles, technology, or live session edits.
 Own fleet assets/exporter and its exact packaging delta; return shared integration
 notes to the integration owner. Prove catalog/property equivalence, rendering,
 real service and cold continuation; record uncovered units and human acceptance.
 Return a scoped verified commit and five-minute mission. No push/PR unless
 separately dispatched, merge, automatic expansion or complete-reskin claim.
```

## Stop and replan

Return concrete source/evidence at independent maps/unloading/newglobalmap,
new phase/economic-role model, new train physics/economic role, changed transport
or registry/routing protocol, non-deterministic simulation, new save schema,
new replicated command behavior, broad economy rewrite, new currencies/markets,
new resource/tech/lore system, or a graphics admission change affecting gameplay.
Routine file/asset choices within the frozen scope do not need another approval.
Renderer-only sprite hooks within existing contracts are permitted; an art task
must not quietly repair pre-existing visual-manager persistence or tick behavior.
A healthy current owner, unmatched checkpoint, changed upstream interface or
shared edit collision pauses dispatch for reconciliation, not duplicate work.

## Planning validation

This delivery changes prose only. Validate local links/source anchors, both
repository linters and diff whitespace. No engine suite, new campaign, graphics,
Antigravity execution, merge or publication is claimed. Keep exact planning commit
and validation result in the delivery response; do not invent a competing handoff.
