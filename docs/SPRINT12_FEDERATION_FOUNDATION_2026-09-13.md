# OpenSpaceTTD Sprint 12 — Federation Identity and Consist Snapshots

Status: **COMPLETE**

Sprint 12 begins the federation-preparation roadmap without changing live
Portal Gate traversal. It gives transferable train consists stable identities
and defines a deterministic, bounded snapshot envelope that later sprints can
hand to a Universe Authority or another server.

## Player outcome

There is intentionally no player-visible behavior in this sprint. Trains,
orders, Portal Gates, savegames, and multiplayer continue to behave as before.

## Identity contract

- A save owns a persistent 128-bit `FederationNamespace`, derived without
  consuming simulation RNG from OpenTTD's savegame identity. Deterministic map
  metadata is the fallback for legacy/test environments without that identity.
- A `GlobalConsistID` combines the namespace with a monotonically increasing
  64-bit sequence.
- Identity is anchored to the train unit that headed the consist when assigned.
  Reordering preserves it; after a split it follows that original unit; after a
  merge the destination identity wins; destruction releases an anchored ID.
- The optional `FIDS` table chunk stores the namespace, allocator counter, and
  deterministic anchor mappings. Older saves need no migration. Invalid or
  stale mappings are discarded after load.

## Snapshot format v1

`ConsistSnapshotCodec` captures only state that can be represented without
global station/company identities or destination map coordinates:

- global consist ID, opaque owner token, and caller-supplied content-manifest
  token;
- ordered engine/wagon/articulated topology;
- engine and subtype IDs, refit and cargo totals, build age, value, reliability,
  breakdown counters, and deterministic random bits;
- direction, speed, acceleration, stopped state, and reverse-running state.

The format deliberately excludes tiles, coordinates, reservations, cached
values, orders, shared-order links, and cargo-packet station provenance. A
non-empty cargo total is marked as having unresolved provenance. Live vehicle
allocation and remote materialisation are deferred.

The canonical byte stream is little-endian, begins with `OSCS`, carries an
explicit version and total length, and ends with CRC-32. Decoding rejects a
different content manifest, invalid fields, empty or oversized consists,
truncation, trailing data, unsupported versions, and corruption. Limits are
256 vehicle parts and 1 MiB per snapshot.

## Automated acceptance

- [x] Namespace derivation is deterministic and consumes no simulation RNG.
- [x] IDs survive reorder/split semantics and destination IDs win merges.
- [x] Vehicle destruction removes anchored identities.
- [x] `FIDS` save/load restores namespace and sequence and prunes stale entries.
- [x] Repeated encoding produces byte-identical output.
- [x] Snapshot encode/decode preserves every supported field.
- [x] Manifest mismatch, checksum corruption, truncation, and invalid cargo are
  rejected.
- [x] Capturing a live consist does not alter its spatial or random state.
- [x] `ninja -C build openttd_test openttd` passes.
- [x] Full `openttd_test`: 161 cases and 14,198 assertions pass.
- [x] CTest: 165/165 tests pass with zero failures.
- [x] Executable help smoke test and `git diff --check` pass.

## Deferred to later federation sprints

- Global station, company, cargo-source, and order-destination identities.
- Universe content-manifest generation and negotiation.
- Portal-entry snapshot retention, network transport, authority handoff, and
  live consist reconstruction on another map or process.
