# Retained seed2026 alternate-start feasibility

**Query-only replan investigation, 30 September 2026. Not an approved start
switch or completed food proof.** The owner authorised this assessment in parallel
with a separately owned generation diagnosis. The original seed11 plan remains
blocked; this investigation changes no generation, economics, authority or state.

## Exact start and native probes

Retained A1 seed2026 `operation.sav` SHA-256:
`5ace1f1f3f1627334034a6d2bcff1584282777d91268b7a43f32d2dce519b038`.
Tick 87,296; £36,473 cash, £100,000 debt, £300,000 loan ceiling; available cash
plus ordinary credit £236,473. The £138,919 A1 figure is a later sampled result,
not this checkpoint's cash. The canonical integrated content hashes are unchanged.

The native binary is the independently reviewed source `abeb8f0045`; execution
head `c8f813127b4a18dd87851a8d7e7a82edaf09ab6c` differs only in delivery
documentation/evidence. Binary SHA-256:
`09eb6676ba5883bf2e689b3b679e8117fd73ac60ea7ae6a4b1d976f33f25e789`.
The paused process loaded a new copy of the original save, with no active client.
Its original `first-freight-status` equals retained A1 `before_reload` exactly.

Two commands cover every existing grain producer/direct Industrial link/Core-town
combination in this save:

```text
connected_economy first-food-plan 3 0 2
connected_economy first-food-plan 10 0 2
```

Both return `legal=false`, `preview_unchanged=true`, processor9, grain link2,
food link1. They take 7.09 and 7.19 seconds, within a 15.87-second entire probe.
Initial/final enriched states are equal; both original and copied save hashes
remain equal to the reviewed hash. Advances, borrowing, designation, construction
and spending are all zero. No operating proof or income progression ran.

[Raw evidence](seed2026-preflight.json), [script/config/log/save archive](seed2026-preflight.tar.gz),
[manifest](seed2026-preflight-manifest.json). The archive preserves the exact
candidate limit and existing native search bounds; no rerun with larger bounds.

## Complete topology and failing approach

| Asset | Native checkpoint fact |
|---|---|
| Grain producer3 | Frontier world2, `(475,342)`, current grain rate7 per256ticks |
| Grain producer10 | Frontier world2, `(637,349)`, current grain rate124 per256ticks |
| Grain public link2 | world2 terminal `(253,344)` ↔ world1 terminal `(765,207)` |
| Food processor9 | Industrial world1, `(928,175)`, recipe404, empty grain/FOOD buffers, capacity100 batches/month |
| Food public link1 | world1 terminal `(259,214)` ↔ Core world0 terminal `(768,30)` |
| Existing receiver Town0 | Core world0, `(749,77)`, population2,366, 55 houses; not yet designated |
| Public admissions | £100 per endpoint admission, both links neutral/public |

The generated assets and links exist. The runtime finds each producer approach,
then fails the grain processor approach. All32 failed station attempts report
`industry=9`, target193277 `(765,188)`, closest distance1 and visited30,000–30,002
states. These are the existing limits: 16 sorted station candidates per leg,
30,000 predecessor-state cutoff and bridge spans up to16. The slight overrun is
one loop expansion, not a changed limit.

The target already has owner16 (`OWNER_NONE`) horizontal track mask1, from public
link3's terminal. It was already present in the original A1 initial state and the
retained operation state. Link2's prescribed outward approach needs vertical track
there. The planner excludes existing rail not owned by company0. Independently,
normal `CmdBuildSingleRail` calls `CheckTileOwnership` on existing rail before
adding track, so the current company cannot add the required vertical track to
this neutral tile. The subsequent native probe below separately executes that
read-only query. Public train traversal does not grant permission to rebuild
neutral infrastructure.

The guarded `first-food-access 3 0 2` probe uses existing native access predicates,
`CFollowTrackRail` and ordinary `BuildRail` `QueryCost`, without a new target search
or changed bounds. Executable source `74f0ea20ed13299445b22c1d0b65d07b23f25736`
is clean; only delivery docs were dirty. Its binary SHA-256 is
`3b6f31534d575a0e6a8c3ef49740c8fd8d8d258741e698aaf70aaf963e7eaf3f`.

All34 actual terminal parts match expected TrackBits and are traversable. The
target also passes `StellarNetwork::CanTraverseTile`. Enumerating actual native
track endpoints finds exactly one external edge:194301 `(765,189)`, TrackY/NW,
towards193277 `(765,188)`, TrackX. Native following fails with `NoWay` (error4);
the vertical-track construction query fails with `STR_ERROR_OWNED_BY` (4058).
Every other terminal edge connects internally or to its direction-enforced gate;
the opposite side of the gate is not a rail approach. Original A1 cold equality,
repeated enriched equality and both save hashes remain unchanged. Advances,
spending, grants, ownership and access-policy changes are zero.

[Native access evidence](seed2026-access.json), [raw probe/archive](seed2026-access.tar.gz),
[hash manifest](seed2026-access-manifest.json).

Increasing search/state bounds or earning more money cannot remove this fixed
target conflict. No attempt was made to modify/adopt public infrastructure, expand
the planner, terraform, author another gate, or change permissions. A method using
other construction capabilities may exist; this is not a universal impossibility
proof. However no alternate compatible external joining edge exists for the
unchanged ground-rail search, regardless of which of its16 station candidates
reaches the boundary. The complete-chain command short-circuits here, so the two later
FOOD legs, compound footprint legality and native whole-chain costs remain
unquoted/unproven. A receiver alone is insufficient to accept this alternate start.

## Funds, timing and demand limits

No full-chain construction, vehicle or operating-reserve quote exists after the
failed route. £236,473 available cash/credit does not establish affordability.
The inherited zero-running-cost CST policy is unchanged; existing output service
running cost820/year is visible in the native snapshot. Neither supports a
profitability claim for the proposed new chain.

The existing planner would require construction plus both ordinary consist quotes
and a reserve of at least£30,000. Its larger reserve derives from current native
running rates, interest at maximum allowed debt, monthly upkeep,20 £100 admissions
(£2,000),25% current grain allocation, ten route tiles/day, two processor months
and two extra reserve months. These are planning allowances, not measured income,
station ratings, congestion or guaranteed waiting times. Route-dependent allowance
and reserve cannot be evaluated without a legal route. They are not tuned here.

The current three-hopper plan carries135 grain. Nominal fill at present rates,
before allocation/travel, is4,938 ticks for producer3 or279 ticks for producer10;
the planner's25% allocation assumption multiplies those byfour. Native monthly
recipe404 consumes two grain for two FOOD, up to100 batches/month with600-unit
input/output buffer caps. A135-grain load can therefore produce134 FOOD initially;
subsequent odd remainders can combine. Supply, ratings and actual throughput still
need later operation proof.

Current Town0 demand is119 FOOD/month, with357 FOOD reserve cap. Planned three
35-unit FOOD wagons hold105. One full arrival cannot fund one119-unit basket;
three full arrivals315 cannot fund three baskets357. At leastfour full-capacity
arrivals would be needed from zero reserve, with actual acceptance/timing measured.
The original criterion is **at least** three paid arrivals **and** three consuming
months, so this is a timing consequence, not a reason to weaken either criterion
or change capacity/demand. FOOD alone sustains the town without proving growth.

## Exact conditional replan and present decision

**A seed-only replacement is not supportable yet.** Keep the authorised seed11
stop and do not launch full seed2026 operation. Parent/owner first needs a concrete
choice about the blocked public-terminal approach: a separately scoped generated
terminal geometry repair, or an explicitly approved alternate ordinary construction method
that produces complete unchanged-state native quotes. Neither is implemented or
presumed authorised by this assessment. A public ownership/permission rule change
would be a material design decision; a future generation repair also needs new
generation/A1 evidence and would not automatically repair this retained save.

If that prerequisite is resolved and independently reviewed, the smallest start
contract amendment would replace only the seed11 checkpoint identifiers/settings
with this exact retained seed2026 operation save/hash, tick87,296 and£36,473/
£100,000 finances; select genuine Town0 and its actual houses, processor9, and only
ordinary routes proven by the complete paused preflight. Retain all existing
no-grant/no-authored-assets/role/research/balance rules, positive cash and native
debt/toll accounting, at leastthree distinct paid FOOD arrivals, three distinct
consuming months, exact cold equality and another arrival/consuming month after
reload, 240×2048-tick and30-minute per-phase bounds, human acceptance Pending.
The existing runner is hardwired to the reviewed seed11 start; a later authorised
implementation would explicitly adapt and re-review its start guards before use.

**Recommendation:** retain existing public traversal and construction ownership
rules. A generally useful bounded planner improvement could reuse already-present,
compatible public track at zero construction cost; it would not add the missing
vertical connection or unblock this saved terminal. This evidence does not justify
a permission exception. Return the concrete overlapping approach geometry to the
owner/generation reviewer for a separately scoped decision and new proof, keeping
this food proof blocked. No bounded planner-only repair has been established for
this exact retained seed2026 start.

This is a reviewable conditional proposal, not a new canonical execution plan or
an approval request that blocks this evidence handoff. No second slice starts.
