# OpenSpaceTTD Sprint 13 — Federation Content Admission

Status: **COMPLETE**

Sprint 13 completes the first strict content-compatibility boundary for future
federated consist transfers. A server can now describe the content assumptions
used to interpret a Sprint 12 consist snapshot, produce its canonical token,
and reject a snapshot created under different content before any vehicle state
is changed.

## Player outcome

There is intentionally no player-visible behavior in this sprint. Portal Gate
traversal, trains, savegames, and multiplayer packets remain unchanged.

## Strict manifest contract

- `UniverseContentManifest` records the network/build revision, landscape,
  dynamic-engine mode, and the effective ordered NewGRF configuration.
- Each NewGRF entry records its four-byte ID, exact MD5, palette,
  simulation-relevant load flags, and parameter vector. Local filenames,
  translated display names, status messages, and other presentation metadata
  are excluded.
- `ContentManifestCodec` captures the active configuration on demand and
  provides canonical encoding, decoding, BLAKE2b-256 token generation, and
  first-mismatch diagnostics.
- The canonical stream is little-endian, begins with `OSCM`, carries version
  and total length fields, and ends with CRC-32. It is bounded to 256 NewGRFs,
  128 parameters per NewGRF, and 1 MiB.

## Snapshot admission

- `ConsistSnapshotCodec::CaptureForCurrentContent` supplies the token derived
  from the active manifest rather than accepting an invented caller token.
- `ConsistSnapshotCodec::DecodeForCurrentContent` rejects a manifest mismatch
  before returning decoded transfer state.
- Existing explicit-token entry points remain available for deterministic
  fixtures and future transport plumbing.
- The `OSCS` snapshot wire version remains 1; Sprint 13 changes no live vehicle
  allocation, portal traversal, or simulation state.

## Automated acceptance

- [x] Repeated manifest encoding and hashing are byte-identical.
- [x] Valid empty and populated profiles round-trip through the codec.
- [x] Revision, landscape, dynamic-engine, NewGRF order, identity, checksum,
  palette, flags, and parameters participate in strict compatibility.
- [x] Corrupt, truncated, malformed, oversized, and unsupported descriptors
  are rejected.
- [x] Current-content snapshot capture and admission accept matching content
  and reject a different token without modifying the train.
- [x] Project build, focused federation tests, full test suite, executable help
  smoke test, and whitespace validation pass.

## Deferred

- Network exchange of manifests and compatibility diagnostics.
- Transfer ledgers, authority handoff, train despawning, and reconstruction.
- Global station, company, cargo-source, and order-destination identities.
- Relaxed or capability-based compatibility profiles.
