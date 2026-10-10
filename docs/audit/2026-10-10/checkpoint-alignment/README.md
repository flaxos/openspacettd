# OpenSpaceTTD checkpoint alignment — 10 October 2026

This record publishes previously local October 7 planning evidence and reconciles
it with the current GitHub stack and resumed Antigravity workspace. Source files,
content and owner saves are unchanged. Earlier dated failure records remain.

## Authoritative branches and owners

| Checkpoint | Exact head | Current use |
| --- | --- | --- |
| Integrated GitHub main | `85c423b4a2dd57e5a2c2cf1f6d29bc2e4667845d` | Merged baseline |
| Draft PR66, `codex/generation-money-comparison` | `7bc29a60cc1bc60b4d555d7500c2ded79c175eaa` | Five commits after main |
| Draft PR71, `codex/first-core-construction-basket` | `1c95f075c0bdd4d6ada912b58cc2993302520a94` | Twenty further commits; PR72 included |
| Local coordinated planning | `a434c99c92408bc955d8edce933446f6ba426a73` | Three-document planning delta published here |
| Local station helper base | `9b971a1b2f1eec2208e64422e8b28aca21e18d2e` | Four-file uncommitted patch; no delivery claim |
| Local gate-art base | `85c423b4a2dd57e5a2c2cf1f6d29bc2e4667845d` | 22 unpublished asset/script files; no runtime claim |

[PR66](https://github.com/flaxos/openspacettd/pull/66) targets main;
[PR71](https://github.com/flaxos/openspacettd/pull/71) targets PR66's branch.
Both remain draft. PR72 merged into PR71 and PR64 into main. The last inspected
exact heads had 20 successful check runs each; these do not cover unpublished
changes. Main's [nightly run](https://github.com/flaxos/openspacettd/actions/runs/37915509491)
retains Intel Mac, MinGW and annotation failures. No merge was performed.

The resumed Antigravity conversation `35dd1f26-adeb-462a-b5b9-f65f98b0d744`
is mapped by its existing read-only local API to `/home/flax/games/openspacettd`.
API initialization metadata retains an older branch/origin label; current Git
and its completed response show PR71 `1c95f075`. The final observed step is
184/completed. It proposed a fresh run from the October 1 source save because
it had not incorporated the recovered October 5 evidence. No message, cancellation,
source edit or duplicate implementer was sent to that session by this task.
Reported build/focused checks are Antigravity's results, not reruns of the helper.
Its broad `[gui]` attempt also recorded failures; do not describe all tests green.

## Recovered cloud proof

Local root: `/home/flax/Documents/Codex/2026-10-05/task/core-helper-evidence`.
The archive `core-basket-20261005-assisted-continuation.zip` matches SHA256
`0d12e707bfe3d9f43578a97d5ad1efa779ea7d38941267f8ace86531557a98d4`.
ZIP CRC passes; all 27 operation and 24 checkpoint ledger hashes match.
The standalone retained final save matches
`89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f`,
tick 304,384, money 3,520,926. Historical cloud task
`01a0f44b-1ea3-7523-8bdc-82f302c4e0b1` was completed; this local archive
is its recovered proof, not a claim about a new container's live contents.

Operation 018 records consumption of 60 FOOD, 20 STEL and 20 BALL at tick
284,995. The growth/passenger multipliers are 1.0 and native growth hook is
observed, but native growth is disabled; population 1,191 and 29 houses remain.
Train 28 restarted in operation 003. Thus one full consuming basket is proven;
actual house growth, repeated baskets, sustained supply, post-helper cold proof,
ordinary-start economics and human UAT remain open. Raw summaries are preserved
locally and selected evidence is in [verification.json](verification.json).
No save was loaded or advanced here.

Live historical cloud/Codespace contents were not inspected. The separately
reported Codespace `animated-fiesta-979wqjr7g4cq7r` has unknown unique content;
its reported deletion time is 11 October 07:40 UTC (18:40 Sydney). No deletion,
retention mutation or assumption that its data are redundant was made.

## Unpublished work preservation

`ost-core-town-helper` patch SHA256
`1c880b0904354e1538abf87fa8be5932111edba36702e399a45df84060e04ac1`
matches the October 7 post-review patch byte-for-byte. It modifies only
`connected_economy.cpp`, `connected_economy.h`, `test_core_construction_basket.cpp`
and `scripts/test_core_construction_basket.py`. It passes diff whitespace checks;
exact-final native tests remain NOT RUN in this task. Its remote points to a
local recovery repository, so upstream equality is not GitHub synchronization.
The historical `/tmp/ost-station-helper-native-20261007` directory is absent;
its cause is unverified. Durable basket proof remains verified.

`ost-gate-art` retains 22 untracked files; all are copied to a verified independent
archive. GRF SHA256 is
`268b9b705504c3d081b35459c39e1c952dbec10c2ff56ba330d20063b5b576b2`.
No C++ runtime hook, clearance acceptance or full graphics overhaul follows.

The old `ost-planning` has five empty loose objects and cannot resolve HEAD.
Its source comparison with healthy recovery found 2,642 identical files, no
unique working paths, and only two differing planning documents. Recovery adds
later text without removing nonempty old lines. Both old versions are copied
and hashed separately; source and Git metadata are archived without repairing
the original. Ignored build/cache outputs are excluded from that archive and
remain untouched. Unreachable historical Git completeness is not established.

Preservation root: `/home/flax/Documents/Codex/2026-10-10/task-4/preserved-checkpoints`.
It contains helper patch, art archive, planning-history bundle and corrupt
planning source/Git archive. These are preservation copies, not accepted commits
of someone else's unfinished code.

## One next source task per track

Gameplay source authority remains the existing `ost-core-town-helper` on
`codex/dev-core-town-station-adapter`, guided by
[its source handoff](../../../handoffs/CORE_TOWN_STATION_ADAPTER.md).
Before ownership transfers from the primary Antigravity session, compare the
current heads/status and preserve any new work. Review native pickup `OT_LOADING`
safety, staged payment/partial failure, exact station receiver and FOOD retarget;
then perform exact-final checks. Do not implement another helper in parallel.

Art authority remains `ost-gate-art` on
`codex/art-wormhole-gate-family-light`. Review/reproduce the asset package before
optional resolver/gate-only renderer work. Fleet production follows acceptance,
not merely checkpoint preparation.

Use this alignment checkout for canonical documentation. The primary Antigravity
checkout is intentionally not switched or fast-forwarded while its session owns
it. No active implementation tree is rebased, reset, stashed or cleaned. When the
owner-controlled session is ready to hand over, select the agreed published
source, confirm clean or checkpointed status, and update only that checkout.
The unhealthy planning checkout is not a source for future edits.

The owner retains merge and material design/human acceptance decisions. Review
PR66 before PR71; after each approved merge, retarget/recheck exact heads and CI.
The documentation change stacks on PR71 while it remains unmerged. A new
repeat/cold campaign needs a replacement `ACTIVE_EXECUTION_PLAN.md` with delivered
helper, exact checkpoint and finite bounds. No expired clock is reset by sync.

## Delivery checks

This is documentation-only. File-description and unused-string linters, local
Markdown links and diff whitespace are checked. No engine build, full suite or
native campaign is rerun. Source, content, save and historical evidence hashes are
preserved; unpublished implementation test status is unchanged.
