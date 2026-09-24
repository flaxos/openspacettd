# Two-server multiplayer federation repair — 23 September 2026

The reported local portal hop was not a transfer to the other server. Client 2's
log recorded `Sync error detected`, and server 2 reported that it left after a
desync. The failed run's authority recorded no transfer. The original failure
artifacts remain in `/tmp/openspacettd-federation-two-clients-20260923/failure-evidence/`.

## Repair

- Gate conversion now uses a server-authored native multiplayer command. It also
  preserves the former local counterpart as an unlinked gate.
- Natural train entry prepares the same checkpoint on every peer. Only the server
  performs HTTP requests; physical departures, incoming snapshot fragments,
  materialization and confirmation use the native lockstep command stream.
- Arrival fragments are bounded to the network packet budget and replay-safe.
  Journal state holds partially received snapshots for save/map transfer.
- Imported wagons emerge at their actual spacing. The train retains its full
  global identity, including its source namespace, through arrival and return.
- Manual `federation_dispatch` and local world-status injection are rejected in
  multiplayer because they bypass replicated state. The older manual-dispatch
  runners are not valid acceptance tests for this workflow.

## Live evidence

`scripts/test_federation_multiplayer.py` launches an authority, two independent
dedicated servers and one ordinary multiplayer client for each. The default uses
SDL's dummy display; `--gui` opens two graphical clients. Servers share a host,
but have separate processes, maps, ports and company pools.

The runner creates disposable 512×128 native-engine fixtures. World 1 starts with
one locomotive and one coal wagon carrying 10 units; world 2 starts empty. After
both clients join Company 1, it converts a local pair into a remote link through
the replicated command. No manual dispatch is used. It checks:

1. Natural entry removes the train from server 1 and materializes it on server 2.
2. Both cars drive clear, are visible in engine state and are spaced eight pixels
   apart. The destination has all 10 cargo units; the source has no train.
3. A native reverse command sends the train back through natural gate entry.
4. The original global identity and all cargo return, with no destination copy.
5. Both clients remain connected, with no desync/assertion in any process log.
6. The authority records two completed transfers, zero cargo in transit and a
   conserved ledger (20 units transferred cumulatively, 10 physical units).

The graphical and unattended runs passed. Checked-in `*.log.txt` logs and `result.json`
record the final graphical run. `session.json` pins the executable SHA-256.
No native-window screenshot inspection or new human federation sign-off is claimed.

## Verification and remaining gates

- The build and all 430 isolated CTests pass. File-description and unused-string
  linters pass; `git diff --check` passes.
- The unfiltered `./build/openttd_test` combined process is **not green**: it
  reaches 293 cases, with 289 passing, then aborts after four failing cases in
  spaceport/Sprint 9 save-load/Sprint 30 cargo initialization. The same failures
  recur with the new federation cases excluded. This suggests existing shared
  fixture state, but a clean baseline binary was not built to prove attribution.
  The logs are retained; do not report the combined suite as passing.
- This fixture has no station orders. It does not prove a scheduled production
  route, money/ownership mapping across different companies, all cargo provenance,
  long or articulated trains, every portal direction, or mixed-namespace schedules.
- Snapshot/checkpoint replay tests are not durable crash recovery. Process kills,
  older-save recovery, blocked destinations, reconnects and separate physical
  hosts remain acceptance work. HTTP polling still blocks the server while a
  request is in progress; authority outages need a bounded responsiveness test.

## Replay with two visible clients

From the repository root, use a new output directory:

```sh
python3 scripts/test_federation_multiplayer.py --output /tmp/my-federation-test --gui --keep
```

On success, the clients and servers remain paused. The returned train faces the
outbound gate so resuming replays the outward transfer:

```sh
python3 scripts/federation_session.py --session /tmp/my-federation-test status
python3 scripts/federation_session.py --session /tmp/my-federation-test resume
```

Watch world 1's train near tile (50, 40) enter its gate and emerge at (50, 40) on
world 2's separate map. Once both cars are clear, reverse it using the destination
train window, or the native command helper while the servers are running:

```sh
python3 scripts/federation_session.py --session /tmp/my-federation-test reverse --world 2 --vehicle 0
python3 scripts/federation_session.py --session /tmp/my-federation-test pause
```

`status` reports the current vehicle ID if it differs. Use `stop` to terminate only
the recorded disposable processes. The helper rejects stale/reused process IDs.
The runner never loads or edits the user's existing game saves. Verified saves,
logs, authority state and process manifest are retained in the output directory.
