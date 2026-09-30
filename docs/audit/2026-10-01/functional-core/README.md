# First Functional Core Supply and Materials I — stopped execution

**ASSISTED FUNCTIONAL: FAIL at offline startup. Human UAT: Pending.** The single
bounded campaign stopped before the runner received its native startup marker.
No assistance command, construction, tick advance, freight observation, HQ or
research command was issued. There is no save or observed ending cash/debt.
Preflight, setup, initial flow/control, both cold checkpoints and independent
replay are **NOT RUN**. Ordinary-start economics remain unproven; the preserved
revised A1 financial failure is unchanged.

The implementation delivers replicated Megacity designation, read-only research
eligibility and guarded offline proof/accounting support. It remains a draft
with the functional outcome blocked. This is not completion of the roadmap row.
No second slice, campaign retry, balance change or merge is authorized here.

## Reviewed ancestry and source custody

Live main was checked before branching and again before delivery:
`91846666f69929443eab04cea63f9f8cfb3bb0be`, containing merged PR65 generation,
PR64 optional art and PR58 maintenance. PR66 remained open at
`d1bb617664dcbb122aaefc1d4175c34ae1a17e2e`. The implementation branch
`codex/functional-core-supply-materials-i` starts at that exact dependency and
targets `codex/generation-money-comparison`. Its existing worker owns PR66 CI;
the correction was neither duplicated nor merged by this task.

The reviewed docs-only PR67 head
`2c5e27125cd7f2e92171d7c203760c74d258442e` was brought forward by a normal
cherry-pick (`876f2e855c`). [ACTIVE_EXECUTION_PLAN.md](../../../ACTIVE_EXECUTION_PLAN.md)
is byte-identical to that reviewed head. No public history was rewritten.

The stopped campaign and initial full gates used frozen implementation
`d7a3a12006efb0e890a3020849eccb12be0aeac1`. Its complete source/binary/config/content
provenance is in the unmodified campaign `evidence.json` in the archive.
`7bb7587d5922b77de37ae4a4244b7b15905f3d1f` adds only the diagnosed Python console
transport repair. A later Doxygen follow-up adds only allowance/parameter/return
comments in `connected_economy.cpp`; it changes no executable code. The manifest's
source hashes and local binaries describe the gated `7bb7587d59` source; the
follow-up comment hash is recorded separately. No engine/test behavior changed
after the failed campaign. Delivery documentation is separate from both source commits.

Necessary historical PR62 hunks were reviewed individually, not imported as a
branch or full tree:

| Hunks | Reason / retained boundary |
|---|---|
| Command IDs/traits, designation handler and GUI wiring | Preserve the existing free action, live server-derived town/world/name/population and world-0 fallback. Separate normal/spectator commands retain native packet company identity. |
| Empty station catchment guard | Prevent an invalid bitmap iterator; require a real own-house receiver in the proof. |
| Designation GUI/command tests and native queue relay | Query/denial atomicity, ordinary/spectator posting, three-process replication and native save/load. This is not a full multiplayer socket/handshake playtest. |
| Optional native freight, monthly use and cash observers | Read-only evidence while armed; no production, price, toll or running-cost rule change. |
| Unique authority save directory helper | Preserve isolated parallel test saves; no shared-file overwrite. |

Old PR62 gameplay construction/proof code and its failed results were not copied
wholesale. PR65 terminal/Core-town fixes and existing ordinary-money guards remain.

## First stop and focused repair

The single invocation was:

```sh
python3 scripts/test_integrated_economy.py --functional-core --seeds 11 \
  --output build/agent-logs/functional-core-primary
```

It failed with `Timed out waiting for FUNCTIONAL_OFFLINE_START` during the
45-second startup acknowledgement wait. The empty native/driver/command logs,
traceback, canonical config and failed report are retained. No pristine checkpoint
was obtained, so generated inventory, route coordinates and financial state
could not be captured. No alternate candidate, seed, budget or full retry followed.

Native console logging writes through a buffered file and flushes when `script`
closes it. The runner now closes startup logging before opening the FIFO bridge,
and frames each command as log-open / native command / log-close. It also holds
partial file lines until a newline arrives, rejecting and retaining a terminal
fragment instead of parsing truncated JSON.

Two focused regressions support this narrow repair: a vanilla 64², one-world SDL
probe using only two echo commands verifies short-marker framing; a synthetic
split-file test verifies complete JSON-line assembly and terminal-fragment
failure. Neither invokes functional operations, grants, candidate searches or
the campaign. Canonical offline bridge, grant guards and end-to-end continuation
still require a newly authorized bounded campaign.

The early research/UI fixture failures are preserved too. Queries now check both
ordinary/integrated modes and absent/existing records. TECH save/load uses a valid
ordinary-content fixture; the real Tech Tree panel is exercised with null-blitter
glyph headers and existing native base-graphics reload setup. This proves the
query-state regression, not human readability or real integrated cold progression.

## Exact assistance and financial limits

| Entry | Amount / observation |
|---|---|
| Owner-approved allowance | One £6,000,000 **virtual in-game** cash grant; offline only. No real-money transaction. |
| Assistance commands issued / amount applied | **0 / £0**. The constructor failed before the grant operation. |
| Recorded native receipts / gross debits | £0 / £0; transaction list is empty. This is not observation of the company's actual ending balance. |
| Expected ordinary profile | £100,000 starting cash, £100,000 debt, £300,000 maximum loan. No runtime snapshot was obtained. |
| Observed ending cash / debt | **Not observed** (`null` in the failed report). Complete financial reconciliation is NOT RUN. |
| Requested simulation advances | 0 initial / 0 cold. |
| Full-chain candidates / replay | 0 / NOT RUN. |

The £1m pre-HQ and £4m all-phase gross-debit caps, £5.1m reserve, native £2.5m HQ
cost and £100k research cost were not exercised. They remain binding. No funds,
cargo, RP, technology or loans were silently added. The inherited CST input
locomotive/hopper zero running cost remains visible in source: railv3 omits
`running_cost_base`; no cost policy was changed. Actual operating accounting is
unproven for this mission.

## Verification and retained evidence

| Check | Result / scope |
|---|---|
| Delivery rebuild at `7bb7587d59` | PASS |
| Full Catch | PASS: 495 cases / 306,272 assertions, exit 0 |
| Sequential CTest | PASS: 510/510, exit 0; includes isolated HQ panel and designation relay/reload |
| Both repository linters / diff check | PASS: exit 0, zero linter warnings |
| Focused native console / split reader / Python compilation | PASS; transport and syntax only |
| Primary assisted campaign | FAIL at startup; £0 assistance issued |
| Functional phases / independent replay / ordinary economic proof | NOT RUN |
| Human acceptance | Pending |

[manifest.json](manifest.json) pins the archive members, source and binary inputs,
local gate results and the exact bounds/status of every unrun mission phase.
[native-proof.tar.gz](native-proof.tar.gz) retains the failed campaign inputs/logs,
focused fixture failures and corrections, focused console regression and full
repository logs. It contains no functional save because none was obtained.

The prior generation archive remains unchanged at SHA256
`8447c671a8f567deb60e50d87c171e8dfd25400df32587edf32d9fa69136022c`.
All 17 tracked optional-art files and the published packs are unchanged:

| Pack | SHA256 |
|---|---|
| integrated_v1 (content version 5) | `b9a6a1c42cbfb46fe29aef7ca105342e62035431bc0788742321e8a747db3605` |
| rail_v3 | `1561bc45c46c356d897dd92d81327445a0edd3107731506dd5db726d8d3c1eea` |
| equipment_v1 | `afc62b4ea9ba8733be4010dd390d30b98acbb668184b09f00561227c1644cf21` |

Optional platform art stays disabled in the campaign profile. Exact delivery-head
remote CI and independent final review are recorded on the draft PR. The
[initial Doxygen gate](https://github.com/flaxos/openspacettd/actions/runs/36787553350/job/110132491417)
failed on 13 introduced documentation warnings; the allowance and helper
parameter/return comments are corrected. Passing
compiler/unit gates cannot turn the stopped mission into a functional pass.

## Safe fresh checkout and manual owner feedback

Use a **new directory**, preserving existing edits/builds/saves:

```sh
git clone --single-branch --branch codex/functional-core-supply-materials-i \
  https://github.com/flaxos/openspacettd.git OpenSpaceTTD-functional-core
cd OpenSpaceTTD-functional-core
git status --short
git log -1 --format='%H %s'
cmake -B build -G Ninja
ninja -C build
```

Use the exact draft head reported on the PR and installed base graphics. Build a
disposable launch config with absolute paths to the three pinned packs; no shared
config or existing save needs modification:

```sh
python3 - <<'PY'
from pathlib import Path
root = Path.cwd()
text = (root / 'demo/integrated_economy.cfg').read_text()
for name in ('integrated_v1', 'rail_v3', 'equipment_v1'):
    text = text.replace(f'openspacettd_{name}.grf =', f'{root}/bin/newgrf/openspacettd_{name}.grf =')
(root / 'build/functional-play.cfg').write_text(text)
PY
OPENSPACETTD_WORLD_COUNT=7 ./build/openttd -x \
  -c "$PWD/build/functional-play.cfg" -G 11 -t 1950 -g
```

This is an **unverified manual mission**, with no observation save or verified
construction coordinates. Allow 15 minutes for feedback; completion is not
promised. Keep the original limits and stop at the first unexpected control or
message. Human Pass/Fail/Partial must identify the exact build/content and steps:

1. Pause immediately after the fresh New Game. Inspect the Core town/public
   terminals and actual £100k cash/debt; record the build and content while the
   company remains pristine.
2. Apply the disclosed assistance while still in that pristine fresh generation,
   using the console once:
   `connected_economy functional-start`, then `connected_economy functional-grant`.
   Record £6m assistance, £6.1m cash and unchanged debt; resume with the normal
   pause control. The grant must precede designation, construction and the first
   save/reload; no second grant is permitted.
3. Use Megacity Overview to designate the town and inspect duplicate protection.
   Use normal quotes, construction, refits and orders for grain → processing →
   FOOD → actual Core-house catchment. Follow native payments and three consumed
   monthly baskets within the original budgets; report the first unavailable action.
4. Inspect native processing, town reserves and repeated monthly FOOD use. Food
   alone does not promise growth; record cash/debt and missing expansion baskets.
5. Buy the Core HQ normally, select Materials I and £100k budget, save before its
   first paid month, quit/reload, then observe completion and Materials II available
   by query/UI only. Turn the budget off; do not start II.
6. Save/quit/reload the completed state and look for retained designation/I, another
   paid FOOD delivery and consumed month. Report manual versus automated steps, elapsed time,
   cash/debt/assistance and first failure. Human acceptance remains Pending until
   this identified-build feedback exists.

## Bounded next choices

Recommended: provide a **new explicit handoff for one seed11 campaign attempt**
after the transport repair, retaining the original cash/search/spending/tick/time
limits. This is an execution retry decision, not another cash exception. The
reviewed plan states: “another full campaign requires explicit handoff/replan.”

Alternatively, review the prerequisite repairs as a partial delivery and pause
the full mission. No FOOD-only or research success has been inferred, and no
choice starts a second slice automatically.
