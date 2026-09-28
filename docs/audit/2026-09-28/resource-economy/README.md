# Resource economy verification — 28 September 2026

[evidence.json](evidence.json) records the frozen executable hash, generation and
cold-reload results, two actual map joins, 447 unit cases / 66,829 assertions,
458/458 CTests and the authored resource economy acceptance. Both repository
linters and `git diff --check` pass. `economy-run.json.gz` retains the full
conservation/delivery trace; `command-replication.txt` records the independent
server/client command-queue test. Human graphical UAT remains pending.

Reproduce with the commands in
[the feature guide](../../../PLAYER_BUILT_RESOURCE_ECONOMY.md#verification-and-reproduction).
The matching authored checkpoint remains in
`build/resource-economy-acceptance/checkpoint.sav`; its hash is in the evidence.
Existing published saves and GRF binaries are unchanged.

The isolated generation test initially inherited a zero cargo-scale setting;
its fixture now explicitly initializes and restores that setting. The first
resource economy run exposed missing research-bonus accounting in the optional
audit; actual rounded bonus output is now counted. Neither repair changes native
industry production. The original showcase's ten-minute visual-growth target
remains unchanged; this new resource-flow fixture checks genuine growth from its
cold start without claiming that showcase timing.
