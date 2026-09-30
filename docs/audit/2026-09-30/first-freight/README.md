# A1 First Sustainable Freight — 30 September 2026

**Human UAT: Pending.** This is bounded ordinary-start automated evidence under
[the approved Option A plan](../../../ACTIVE_EXECUTION_PLAN.md), not A2, an
indefinite whole-economy proof or graphical acceptance. All three representative native seeds pass the frozen-build proof below.
Discovery and economic failures remain retained; exact delivery-head checks are
recorded separately from this long proof.

## Authority and changes

The owner approved GPT-6.1 Sol Codex taking over implementation; only executor
identity supersedes the checked-in Antigravity wording. PR53 Option A at
`a683e0b08b9830490daea415749e2df7912829ff` is an ancestor of the reviewed base
`309dbe92a3e523b870a3c33a3493c1857829d80d`. The branch also safely merges reviewed
main `a8cae8f6d9fc73b10c89b041ec307e3d414f9b28` (PR56: a documentation tab and
the separate ASan diagnostic helper). The earlier base incorporates merged PR54's
separate prefab-hash lifetime/script-mode CI repairs. A1 does not duplicate those
changes or cherry-pick replacement PR55.

The existing `connected_economy` console harness has a guarded ordinary-start
mode: native company startup, stable bounded route previews, ordinary construction,
vehicles/refits/coupling/orders, normal borrowing and normal ticks. It refuses
joined peers and authored starting player assets. It does not invoke
`prepare-integrated`, adopt neutral assets, inject cargo/cash/research, terraform
an authored route, commission gates or join federation. New snapshot/toll observers
add no persistent state or gameplay accounting. The game generator, economic
roles, balance, content and save chunks are unchanged.

`Engine` now forwards its requested seed to native `-G`. The generation regression
runner forwards `--seed`, and a subprocess-argument regression covers 11/101/2026.
The first-freight runner records the actual native generation seed, exact profile,
content/binary hashes, costs, buffers, captured orders/route/custody/finances,
failures, saves and cold-process continuation. Candidate discovery considers both
iron/steel and grain/food independently for each seed. A deterministic weighted
search finds feasible routes; it does not establish globally cheapest construction
or all-seed reachability.

## Corrections and preserved failures

The 28 September generation reports labelled 11/101/2026 all used native seed11:
`Engine` hardcoded `-G 11`. Their normalized state hash is
`c2775145d049d58e023a41e371ef534f4d663fc44d6a930aaf43772a2170c16c`.
Those historical files remain unchanged; their three-seed claim is superseded by
this correction. Fresh coverage must use both actual native seeds and the manifest.

[Superseded diagnostic manifest](superseded-diagnostics.json) and
[raw diagnostics archive](superseded-diagnostics.tar.gz) retain the initial lost
train, bounded search failures, the native bridge QueryCost/full-cost correction,
an interrupted unload-fragment counter run and the clean `925c47e1f4` run.
That run passed seed11 but failed route discovery on actual seeds101/2026.
None is final three-seed coverage. The unverified £15–20k estimate is disproven
by the measured seed11 route, which costs £75,772 on that initial implementation.

A read-only route audit found valid seed101 terminal bounds and short dry-bank
crossings through its water basin. Native rail previews can succeed by including
river clearance while the ground-only planner excludes water; its old bridge
trigger then missed these crossings. Commit `41c23454c1` corrects only that trigger.
The wider-bridge experiment was abandoned; its patch/logs are preserved in the
archive. Final coverage reruns all three fresh seeds after the correction.

## Reproduce

Build the existing configured checkout, then run from the repository root with
its ordinary content/data environment. Tests/engine processes run sequentially.
Do not overwrite an existing evidence directory:

```sh
ninja -C build -j2
python3 scripts/test_acceptance_seed.py
python3 scripts/test_integrated_economy.py --first-freight --steps 240 \
  --seeds 11 101 2026 --output build/owner-a1-proof
./build/openttd_test
ctest --test-dir build --output-on-failure -j1
python3 .github/file-descriptions.py
python3 .github/unused-strings.py
python3 .github/script-missing-mode-enforcement.py
git diff --check
```

Gameplay settings are exactly [demo/integrated_economy.cfg](../../../../demo/integrated_economy.cfg):
1024², seven Mito–Merredin worlds, temperate/Tgen, year1950, ruleset1,
OST01v5 integrated industry, railv3/equipmentv1, initial cash/loan£100,000,
maximum loan£300,000, low vehicle/construction costs, no inflation/breakdowns/
disasters/competitors, ordinary generated neutral public CST gates£100 per admission.
Automation resolves GRF paths and uses `min_active_clients=1` to freeze unsolicited
ticks; it does not change gameplay difficulty. Saves are synchronous and original
files are not overwritten. All gate tolls, native running/interest/other charges
and processor input/output capacities belong in each seed result.

## Acceptance boundaries

Primary generated processor input consumption is distinct from output warehouse
storage. The modest next step transports real processor output to an ordinary
£75,000 hub using its own paid rail/depot/train/orders. Warehouse storage earns no
transport revenue and is not final steel/food consumption, player research or A2.
The combined service must remain cash-positive and profitable over the recorded
post-reload window. Unbounded company/world stockpile capacity is existing behavior;
this does not establish indefinite economic demand or a globally balanced economy.

The current CST input locomotive/hopper has zero observed native running cost:
railv3 sets `running_cost_factor` but omits `running_cost_base`; native running cost
skips the invalid base price. Record that zero honestly. The ordinary output
locomotive incurs native running costs, and loan interest, both-direction tolls
and other charges still apply. A positive CST running-cost policy would require
separate balance approval and fresh profitability proof; A1 does not choose it.

Cold equality applies to every **captured** snapshot field, not every serialized
native field. Native save chunks remain responsible for uncaptured packet metadata,
signals, footprints and fractional money. No human pass follows from this proof.
The bounded planner excludes water reclamation, long structures and terraform;
a search failure is not proof that no human-built route exists.

## Owner mission — 5–10 minute timebox

Objective: establish and observe repeat iron income through the generated public
backbone using ordinary New Game controls. Use seed11 and the exact profile above.
Fast-forward is needed: seed 11's three input loads take 36.86 nominal
normal-speed minutes, and three reload loads take 43.32. Manual construction time
has not been measured; stop at 10 minutes and report the first unmet step rather
than treating an automated checkpoint as fresh graphical acceptance.

1. Launch `./build/openttd -x -c demo/integrated_economy.cfg`; choose **New Game**,
   the seven-world Mito–Merredin preset, seed11 and the recorded integrated trio.
   Verify cash/loan£100,000 and max loan£300,000; pause while building.
2. Locate Clonclurry's generated iron mine and Merredin's accepting steel mill
   using world/industry controls. Inspect the generated public gate connection
   and £100 admission in each direction. For seed 11: mine `(115,348)`, mill `(525,235)`, source gate `(254,343)`
   and destination gate `(764,217)`. Stations start at `(122,344)` and `(532,231)`;
   source depot `(121,344)`. The exact native rail/bridge preview in
   [seed-summary.json](seed-summary.json) is a construction reference, not granted assets.
3. Build two 2-tile stations, connecting ordinary rail/wooden bridges to the public
   terminals and a depot. Buy the CST Pioneer steam engine plus three ore hoppers
   (135 iron in total); set Full Load/No Unload at mine and No Load at mill, then release it.
4. Fast-forward and observe at least two loads, money increases, actual mill input
   consumption and steel output. Inspect finance/tolls and the £86k-class ordinary
   output-to-warehouse option plus remaining normal loan headroom.
5. Save to a new filename, exit, reload and observe another delivery. Report
   pass/fail, build/seed/content, elapsed wait, first unexpected action/message,
   cash/loan before/after, and any unreadable or inaccessible control. A failure
   to finish within the timebox is useful UAT evidence; graphical acceptance stays
   Pending until the owner reports it.

A saved automated construction/operation checkpoint is available for optional
route inspection, clearly separate from the required fresh New Game mission.

## Final results

The clean long-proof source is `14bcbcb5e606aea4fc8ccc1c320dff994c0296d6`.
All three actual native seeds pass with unchanged content/settings and a Pioneer
plus three normally purchased 45-unit hoppers (135 total, exactly two platform tiles).
Each delivers 405 iron before expansion and 405 after cold reload. No starting
assets, research, cash, cargo or new gate commissioning are authored. Grain/food
opportunities were considered independently; these successful routes use iron/steel.

| Actual seed | Mine → mill tile / public link | Starter cost | Initial net cash | Output step | Reload net cash | Final cash / debt / headroom |
|---:|---|---:|---:|---:|---:|---|
| 11 | 356467 → 241165 / 2 | £80,681 | +£145,634 | £86,432 | +£141,512 | £219,780 / £100,000 / £200,000 |
| 101 | 327169 → 176459 / 2 | £112,267 | +£24,537 | £87,152 | +£10,080 | £24,795 / £190,000 / £110,000 |
| 2026 | 495901 → 188782 / 3 | £83,191 | +£107,098 | £87,180 | +£102,446 | £138,919 / £100,000 / £200,000 |

Seed 101 borrows £30,000 for startup and another £60,000 for the output step;
seeds 11/2026 retain the initial £100,000 debt throughout. Minimum sampled cash is
£17,877 / £6,314 / £15,367 respectively. No operating-phase borrowing changes debt.
Every interval between complete 135-unit input loads gains cash. All routes pay
£600 outbound and £500 return over the recorded six loaded deliveries; the sixth
empty return has not completed at the final checkpoint. Each admission is £100.

| Seed / phase | Gross freight revenue | Train running | Interest | Other (includes gate toll) | Net cash |
|---|---:|---:|---:|---:|---:|
| 11 starter | £153,034 | £0 | £6,000 | £1,400 | +£145,634 |
| 11 reload, both trains | £153,276 | £2,922 | £7,167 | £1,675 | +£141,512 |
| 101 starter | £38,571 | £0 | £12,134 | £1,900 | +£24,537 |
| 101 reload, both trains | £37,365 | £4,476 | £20,584 | £2,225 | +£10,080 |
| 2026 starter | £114,690 | £0 | £6,167 | £1,425 | +£107,098 |
| 2026 reload, both trains | £114,018 | £2,922 | £7,000 | £1,650 | +£102,446 |

The short output-start intervals cost £253 / £403 / £254 respectively, earn no
warehouse revenue and move real steel. Expense totals handle every yearly ledger
rollover; subtracting only the final three-year ledger would lose older costs.
The currently zero CST input running cost is an inherited content limitation above.

Final custody: all seeds deliver 810 iron. Seed 11 consumes all 810 and produces
405 steel: 320 warehouse + 15 train + 47 station + 23 mill. Seed 101 consumes 674
with 136 mill input and produces 337 steel: 320 warehouse + 14 train + 3 mill.
Seed 2026 consumes 674 with 136 mill input and produces 337 steel: 320 warehouse +
15 train + 2 mill. Input capacity is 600 iron; output capacity is 300 steel.
Each mill retains output batch limit 100 and substantial free output capacity.
Steel consumption/research is zero; this is real raw consumption plus output relief.

Normal-speed three-load timings (starter / reload): seed 11 36.86 / 43.32 minutes;
seed 101 57.14 / 66.36; seed 2026 37.79 / 43.32. These are simulation timings, not
measured human construction or desktop fast-forward times.

[Compact per-seed metrics and full route previews](seed-summary.json) ·
[Manifest with exact hashes](native-proof-manifest.json) ·
[All raw JSON/log/config/save evidence](native-proof.tar.gz) ·
[Independent review](independent-review.md) ·
[Retained one-wagon result](one-wagon-result.json) ·
[Retained one-wagon raw evidence](one-wagon-evidence.tar.gz).
The actual generator seed is recorded both in each native snapshot and the manifest;
no older labelled report is used as representative coverage.

Final delivery verification rebuilds and tests the delivery commit after this evidence
commit. Exact SHA, build/unit/isolated CTest/linters/seed-regression/SDL/cold-load results
and remote CI are recorded in the draft PR and recoverable
`build/agent-logs/a1-final-verification.json` plus its per-command logs. The long proof
commit remains distinct: intervening changes affect only evidence/docs and the
separate API documentation/ASan helper. The docs-check follow-up adds Doxygen
contracts and separates the equivalent station/depot declarations; no executable
behavior changes. A1 runners and gameplay config remain byte-identical. No redundant
long soak is claimed at a comment-only delivery SHA.
