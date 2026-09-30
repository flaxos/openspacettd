# First Functional Core Supply and Materials I

**Single next gameplay slice for reviewed handoff — 1 October 2026 (Australia/Sydney;
30 September UTC). Planning/docs only in this task.** The owner approved
functional progression before profitability/pacing: prove player construction,
delivery, processing, town demand, a next unlock and save/reload; record real
costs/revenue and label any cash assistance. Command legality, cargo/financial
accounting, determinism and save integrity remain hard gates.

The owner also instructed the parent to start the first bounded slice as soon as
its reviewed plan is ready. This is that single implementation handoff; no extra
routine approval is required after review. This task remains planning/docs only
and authorizes no merge. It replaces the
[generation plan preserved at its merged revision](https://github.com/flaxos/openspacettd/blob/91846666f69929443eab04cea63f9f8cfb3bb0be/docs/ACTIVE_EXECUTION_PLAN.md).
The [roadmap](PROJECT_STATUS_AND_ROADMAP.md#functional-first-direction--1-october-2026)
sets current acceptance priority; the [reassessment record](audit/2026-10-01/functional-replan/README.md)
pins ancestry and evidence. Older stops remain facts about their original runs,
not a veto on this new proposal. Human UAT remains Pending.

## Outcome and finish line

In one fresh generated seed11 company, build two paid rail services through the
existing public backbone: grain farm → food processor → real Core-town houses.
Observe native payments, processor conversion and three distinct monthly food
baskets consumed. Buy a Core HQ, research Materials I through its normal cash
budget and expose Materials II as the next selectable project. Cold-reload,
retain that unlock, and observe another FOOD delivery and consuming month.

FOOD does **not** unlock research: the HQ and paid research are a separate action
in the same company. Cash assistance pays eligibility/costs; it proves neither
that food funded the HQ nor that expansion is earned. FOOD alone sustains the town
without growth. Stop before Materials II research, steel production, material
kits, resource funding, gate commissioning or a second slice.

Later delivery needs this bounded functional proof, independent review, applicable
code/content gates, a scoped PR and the human mission below. Missing functional
steps remain partial even if unit tests pass. Profitability is measured separately;
neither automated proof nor a prebuilt observation save establishes human build UAT.

## Current base and reusable systems

Fresh main at planning intake is `91846666f69929443eab04cea63f9f8cfb3bb0be`:
A1 PR60, generation plan PR63, optional art PR64 and generation PR65 are merged.
PR65 was owner-merged at 2026-09-30 21:22:47 UTC. Native T+C+D+S proof passes;
platform compilation still needs the separately owned
[PR66 portability repair](https://github.com/flaxos/openspacettd/pull/66).
Its published dependency head is `d1bb617664dcbb122aaefc1d4175c34ae1a17e2e`,
based on `91846666f6`. Its delivery worker owns exact-head CI observation; consume
that handoff rather than start another poller. Do not duplicate its
`connected_economy.cpp` edit or call the build fully green prematurely.

PR62 merged only into the old A1 dependency branch at
`10856e709e492168f5c3fccb89ae4a27daf16643`, not main. Its
[pinned partial work](https://github.com/flaxos/openspacettd/tree/78a30686c3467306b24dc1f0a6cf215f66f484dd)
is a reference: port only individually reviewed necessary command/harness hunks
in a later main-based implementation. Do not merge/cherry-pick the full branch or
import obsolete docs/history. Preserve PR58 maintenance, PR59 history and PR64 art.

- [Integrated economy](UNIFIED_COMMONWEALTH_ECONOMY.md) already processes
  2 GRAI → 2 FOOD without research, routes physical cargo and consumes Core baskets.
  Native warehouses have priority; no warehouse may intercept the town receiver.
- [Megacity GUI](../src/portal/megacity_gui.cpp) still calls registration directly.
  Route its existing Designate action through a native replicated command before
  counting player-accessible consumption. Preserve free designation, eligibility,
  derived town/world/population and duplicate prevention; no new permission policy.
- [HQ eligibility](../src/portal/corporate_hq.cpp) is £5m current cash; its
  [command](../src/portal/portal_cmd.cpp) charges £2.5m. Integrated mode waives legacy
  three-phase presence, not cash or the Core role.
- [Materials I](../src/portal/tech_tree.cpp) needs an HQ, no prior research and
  100 RP at £1,000/RP; its integrated kit is empty. Materials II requires I and
  250 RP. Use native project/budget commands and monthly processing, never direct
  research-point or unlock restoration calls.
- `TechTreeManager::CanResearch` currently inserts default company state during
  eligibility queries, including the failed Materials II check and HQ GUI paint.
  The default record has an invalid company ID and disappears on load. Make this
  query read-only before the campaign, preserving all eligibility/cost rules.
  Focused regressions must show successful queries, rejected prerequisites and
  GUI inspection preserve both absent and existing research state. Do not hide
  the defect by pre-seeding state/budget or normalizing away a missing record.
- Reuse the [runner](../scripts/test_integrated_economy.py), shared
  [Engine](../scripts/test_wp11_slice.py), native snapshots/audits and
  [save workflow](SAVEGAME_AUTHORING.md). Authored progression/unit fixtures prove
  subsystems, not this generated start. No new economy or persistence model is needed.

## Starting state and the only assistance

Use the [canonical profile](../demo/integrated_economy.cfg): native seed11, 1024²,
seven worlds, temperate/Tgen, 1950, ruleset1/OST01v5, railv3/equipmentv1, ordinary
£100k cash/£100k loan and £300k maximum. Preserve settings, role/phase distinctions,
terrain, public ownership and tolls. Pin source/binary/content/config hashes. Keep
optional art disabled for this comparison. No federation is needed.

Start a **new** game, prospectively replacing the old missing-Core-town food start.
Keep a pristine checkpoint before spending; never repair/fund/overwrite the failed
A1 checkpoint or old food saves. Measure native generated inventory. Do not author
cargo, trains, track, towns, population, stations, stockpiles or research.

Cash-only test allowance: **one £6,000,000 cash grant before construction**, giving
£6.1m cash and unchanged £100k debt. This finite test budget follows the approved
cash-assistance principle; it changes no normal starting-economy setting. Record
the native transaction and before/after state. Reject repeat grants, grants on
reload and use on arbitrary saves; keep debt fixed throughout the assisted run.

Use the existing [native MoneyCheat command](../src/misc_cmd.h) in isolated offline
setup. It is `Offline`: the dedicated-server Engine cannot simply invoke it.
Minimal offline setup/observation support may create the labelled checkpoint,
then a separately guarded runner may continue it. Preserve ordinary A1/generation
guards. Do not remove command flags, assign money directly, enable infinite money,
change the loan ceiling or introduce a general deity/authority bypass. If this
path cannot be made reliable within the narrow harness scope, stop and report
that finding rather than invent another funding mechanism.

The grant is booked by native accounting as Other; retain that ledger and separate
it from freight income/operating net. Reconcile `ending cash = starting cash +
assistance + actual receipts - actual debits`, with unchanged debt. Subtracting the
grant from final cash does not prove an ordinary company could have paid the same
sequence or met HQ eligibility. No recurring refill is permitted.

No extra exception is assumed. Tech/RP/material/cargo grants, receiver/population
authoring, role changes, cost/loan/toll/running-cost edits, unauthorized neutral
construction and save migration require a new explicit owner decision if proposed.

## Bounds and failure handling

These limits govern the reviewed implementation handoff; this docs PR runs no gameplay.

| Work | Hard limit / required result |
|---|---|
| Preflight | Seed11 only; at most two full-chain candidates in stable ID order, with selection reason. Quote both legs, house receiver, vehicles/refits and costs before spending. Retain planner caps: 16 station candidates, 30,000 predecessor states and 16-tile bridge span per call. Generation/preflight at most 30 minutes. No seed sweep. |
| Setup/build | One chain, native paid commands and designation. At most £1m gross debits before HQ (including elapsed charges), retaining at least £5.1m from £6.1m before receipts. All-phase gross debits at most £4m, including £2.5m HQ and £100k research. Thirty-minute setup/build wall limit. |
| Initial proof | At most 240 advances × 2048 native ticks and 30 minutes wall time, whichever comes first. Three separate positive FOOD deliveries/payments, three distinct consumed monthly baskets and research ready for completion; include the negative flow control below. |
| Cold proof | Two cold loads in fresh processes: first before Materials I completes, second after completion. One shared cap of 240 × 2048 ticks/30 minutes for both loads, paid completion and another FOOD delivery/payment/consuming month. No extra grants/loans or rebuilding. |
| Repeatability | One independent fresh replay only after a successful primary run, identical cash assistance/commands/tick schedule and phase caps. Compare semantic state/RNG, custody, receipts/debits, consumption and unlocks. Whole native campaign at most four hours wall time. |
| Failure | First hard defect or exhausted bound stops the campaign. Preserve partials and NOT RUN cases. No automatic alternate seed, rebuild strategy, tuning, full-campaign retry or budget extension. Focused regressions may support a diagnosed repair; another full campaign requires explicit handoff/replan. |

The ordinary-start comparison is a read-only quote/cash/loan feasibility report
from the pristine state, not another income grind or an economic pass. The revised
A1 failure remains the operating comparison. Seeds101/2026 retain generation-only
proof on these layouts; no three-seed food/economic coverage is implied.

**Hard functional stops:** illegal/bypassed command, absent real receiver, wrong
custody/payment, duplicate cargo/cash, research bypass, nondeterminism/desync,
invalid terrain, lost/crashed train, cold mismatch or unreachable UI action.
Money cannot waive these. A legal chain not found within the cap is a bounded
feasibility failure, not proof of universal impossibility.

**Financial stops:** budget exhaustion, unexpected unaffordability/HQ eligibility
failure or bankruptcy stops that run. Keep functional partials and measured losses.
Negative operating net alone does not fail a completed functional mission; never
tune prices or refill cash to complete it. **Time stops:** an exhausted timebox
is partial evidence and a pacing/usability finding, not permission to grant
RP/cargo, advance dates directly or mark human acceptance passed.

## Required controls and evidence

1. **Commands:** query/execute cost parity, failed-command atomicity, role/phase/
   ownership/house catchment and actual GUI wiring. Separately prove designation
   server/client replication without relaxing offline-only funding. Manager calls
   or a headless button test alone do not prove human usability.
2. **Flow/demand:** identify producer, processor, both services, receiver TownID/
   own houses, gates, labels and capacities. Reconcile initial inventory,
   production, all custody, 2:2 conversion, reserves, consumption and explicit
   loss/discard. No warehouse diversion. In a disposable copy pause FOOD until
   reserves fall below the current monthly basket and consumption stops (a
   remainder may remain). Require no consumption or unexplained reserve change
   while supply is stopped, then restart and observe recovery;
   this control shares the initial phase's tick/wall budget. If incomplete,
   report incomplete proof, not invisible stock refill or extended time.
3. **Money:** record every native receipt/debit, research, interest, fee, return
   toll and running cost across annual ledger rollover. Show assistance separately,
   including native Other entries. Keep the inherited zero CST input running cost
   visible while accounting for other costs; state sampling/tick coverage limits.
4. **Unlock:** without HQ, research rejects; with HQ but without Materials I,
   Materials II rejects for its prerequisite. Select I and £100k/month through
   native commands. Observe its actual debit/100 RP/monthly completion after the
   first cold load, then Materials II selectable by query/UI only. Turn the budget
   off and do not start II. No food-to-tech prerequisite is invented.
   Since £100k buys all 100 RP in one month, the first save is before that first
   paid month, normally at 0 RP. This proves project/budget persistence and later
   completion, not nonzero partial-RP persistence.
5. **Persistence/determinism:** pause clocks before snapshots. Compare captured
   map/roles, ownership, orders/vehicles, inventory, designation/reserves/consumption,
   cash/debt/ledger, project/budget/RP before and after each cold load. Complete
   research normally, save the finished state, reload and prove retained unlock
   and further food service. Compare replay at identical observation points;
   normalize only documented serialization differences, not unexplained mismatches.
6. **Custody/labels:** preserve raw command/query logs, failures, time/tick bounds,
   save hashes and source/binary/content provenance. Use separate `ordinary-start`,
   `cash-assisted-functional`, `fixture-regression` and `human-uat` labels. Every
   automated result is PASS, FAIL, PARTIAL or NOT RUN with scope; human status is
   independently Pending/Pass/Fail/Partial. Costs/net/waits stay visible on a PASS.

## Six-step human mission

Later implementation supplies the verified build/content, seed, construction
coordinates/quotes and labelled save. Allow 15 minutes for feedback; completion
time is not promised. Fast-forward changes waiting, not production/research rules.

1. Start the specified fresh seed11 profile, inspect Core town/public terminals
   and £100k cash/debt, then apply and record the single disclosed £6m test grant.
2. Through normal controls designate the town, build the quoted grain and FOOD
   services and set refits/orders. Inspect actual costs and own-house catchment.
3. Follow grain into processing and FOOD to town; see native payments and repeated
   monthly food use. Inspect missing expansion baskets; food alone promises no growth.
4. Buy the Core HQ, select Materials I and £100k budget. Save before completion,
   quit/reload, then see paid research finish and Materials II become available.
5. Save/quit/reload the completed state; see the retained unlock, another FOOD
   payment and consuming month with no second grant.
6. Report Pass/Fail/Partial, exact build/save/content, manual versus prebuilt steps,
   elapsed time, cash/debt/assistance and first unexpected message/control. An
   operation save proves observation only; manual construction UAT remains separate.

## Decisions and implementation handoff

Execute this cash-only mission first in the parent's separate task after review.
The £6m allowance, £4m spending cap and offline setup bound the approved cash-only
assistance; they change no game-economy policy. No routine owner reconfirmation is
needed. No further product/architecture exception is assumed.
If preflight fails these limits, return the blocker and two bounded choices:
specifically revise the cash/time limit while retaining the outcome, or deliver
a smaller FOOD-only partial outcome with research explicitly unproven. Any
tech/cargo/role/rule exception requires a separate owner decision; silence is not approval.

Refresh main/PR66 once for the implementation handoff and consume its delivery
worker's checks. If PR66 is still unmerged, create the separate functional branch
from exact `d1bb617664dcbb122aaefc1d4175c34ae1a17e2e` and target
`codex/generation-money-comparison`; report that dependency explicitly. If owner-merged,
start from fresh main after verifying it contains that correction. Do not merge
PR66, duplicate its source edit or open a main PR carrying its unmerged delta.
Bring this docs-only plan commit forward normally if needed; preserve public history
and PR64 art. Review the necessary PR62 hunks individually, then implement only
this mission's integration/evidence using `ost-dev`, `ost-uat` and `ost-deliver`.
The only known integration repairs are command-based designation and read-only
research eligibility, plus the narrow guarded proof/observation support above.
Do not broaden into earned A2 expansion, federation, dynamic markets, true multi-map,
narrative/Silfen/crisis work or balance. Reassess after this one delivery and human
feedback; do not automatically start the next roadmap row.
