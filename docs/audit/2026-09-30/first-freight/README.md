# A1 First Sustainable Freight — 30 September 2026

**Human UAT: Pending.** This is bounded ordinary-start automated evidence under
[the approved Option A plan](../../../ACTIVE_EXECUTION_PLAN.md), not A2, an
indefinite whole-economy proof or graphical acceptance. Final seed results and
verification are recorded below when complete; failures remain retained.

## Authority and changes

The owner approved GPT-6.1 Sol Codex taking over implementation; only executor
identity supersedes the checked-in Antigravity wording. PR53 Option A at
`a683e0b08b9830490daea415749e2df7912829ff` is an ancestor of the reviewed base
`309dbe92a3e523b870a3c33a3493c1857829d80d`. That base incorporates merged PR54's
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
python3 scripts/test_integrated_economy.py --first-freight --steps 120 \
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
Fast-forward is needed: the initial seed11 proof's three loads take34.10 nominal
normal-speed minutes, and three reload loads take38.71. Manual construction time
has not been measured; stop at10minutes and report the first unmet step rather
than treating an automated checkpoint as fresh graphical acceptance.

1. Launch `./build/openttd -x -c demo/integrated_economy.cfg`; choose **New Game**,
   the seven-world Mito–Merredin preset, seed11 and the recorded integrated trio.
   Verify cash/loan£100,000 and max loan£300,000; pause while building.
2. Locate Clonclurry's generated iron mine and Merredin's accepting steel mill
   using world/industry controls. Inspect the generated public gate connection
   and £100 admission in each direction. Use the final seed11 coordinates and
   route preview retained below as a construction reference, not granted assets.
3. Build two2-tile stations, connecting ordinary rail/wooden bridges to the public
   terminals and a depot. Buy the CST Pioneer steam engine plus one ore hopper
   (45iron); set Full Load/No Unload at mine and No Load at mill, then release it.
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

Pending completion of the corrected frozen-build three-seed run and delivery checks.
