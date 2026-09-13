# OpenSpaceTTD Sprint 14 — Federation Identities and Consist Snapshot v2

Status: **COMPLETE**

Sprint 14 completes the remaining requirements of **Phase F1: Federation Preparation (Architectural Decoupling)**. It establishes globally unique identities for companies, stations, cargo sources, and order destinations, upgrades consist snapshot serialization to wire format v2 with cargo provenance and routing orders, maintains full backward compatibility for v1 snapshots, and extends savegame `FIDS` serialization with stale identity pruning.

With Sprint 14 complete, the OpenSpaceTTD codebase is fully prepared for **Phase F2: Federation Prototype (Technical Spike)**.

---

## Player Outcome

There is intentionally no player-visible behavior change in this sprint. Single-map gameplay, portal gate mechanics, vehicle physics, in-game economics, and existing savegame loading continue to behave identically.

---

## Architectural Changes & Global Identity Contracts

### 1. Global Company Identity (`GlobalCompanyID`)
- Combines the 128-bit `FederationNamespace` with a monotonic 64-bit sequence counter.
- Provides `ToOwnerToken()` conversion for opaque 32-byte authorization tokens across server boundaries.
- Tracked via `FederationIdentityRegistry::_company_mappings` with lazy allocation via `GetOrCreateCompany(CompanyID)`.
- Automatically released upon company liquidation via `Company::PostDestructor`.

### 2. Global Station Identity (`GlobalStationID`)
- Combines `FederationNamespace`, monotonic 64-bit sequence counter, and `WorldID` world attribution resolved via `PlanetManager::FindRegionByTile(st->xy)`.
- Tracked via `FederationIdentityRegistry::_station_mappings` with lazy allocation via `GetOrCreateStation(StationID)`.
- Automatically released upon station demolition via `BaseStation::~BaseStation()`.

### 3. Global Cargo Source Provenance (`GlobalCargoSourceID`)
- Tracks cargo origin with `FederationNamespace`, origin station ID, `SourceType` (`Industry`, `Town`, `Headquarters`, `Other`), monotonic source sequence, origin `WorldID`, and origin tile coordinates (`x, y`).
- Allows consist capture to resolve `CargoPacket` origin data via `CreateCargoSource(StationID, Source, TileIndex)` and clear `cargo_provenance_unresolved` when resolved.

### 4. Global Order Destination Identity (`GlobalOrderDestinationID`)
- Represents consist routing destinations across worlds:
  - `OrderDestinationType`: `Station`, `Waypoint`, `Depot`, `PortalGate`.
  - Encapsulates target `GlobalStationID` or destination portal coordinates and world attribution.
- Supports serialization of goto order sequences without coupling to local tile indices.

---

## Consist Snapshot Wire Format v2

- **Wire Version:** Bumped to `CONSIST_SNAPSHOT_VERSION = 2`.
- **Snapshot Header:** Includes `GlobalCompanyID` and serialized list of `GlobalOrderDestinationID` goto orders.
- **Unit Envelopes:** Each `ConsistSnapshotUnit` now carries resolved `GlobalCargoSourceID` provenance.
- **Dual-Version Backward Compatibility:** `ConsistSnapshotCodec::Decode` transparently accepts both v1 and v2 wire streams:
  - V1 snapshots decode with empty company identity, empty order list, and cargo flagged with `cargo_provenance_unresolved = true`.
  - V2 snapshots decode with full company, provenance, and order fidelity.
- **Security & Bounds:** All limits (256 vehicle units, 256 orders, 1 MiB stream payload, CRC-32 integrity validation) are strictly enforced.

---

## Savegame Compatibility (`FIDS` Table Chunk)

- Extended `SlFederationIdentity` chunk handler to serialize multiple record kinds:
  - Kind 0: Federation namespace metadata.
  - Kind 1: Consist anchor mappings.
  - Kind 2: Company mappings.
  - Kind 3: Station mappings.
  - Kind 4: Cargo source sequence mappings.
  - Kind 5: Monotonic allocator sequence counters (company, station, cargo source).
- During save load (`Load`), `PruneStaleCompanyMappings()` and `PruneStaleStationMappings()` safely discard mappings whose local pool entities no longer exist.
- Older savegames lacking kinds 2–5 load cleanly without error or data regression.

---

## Automated Acceptance & Verification

- [x] `GlobalCompanyID` allocation, token conversion, destruction release hook, and stale-mapping pruning pass.
- [x] `GlobalStationID` allocation, world attribution via `PlanetManager`, destruction release hook, and pruning pass.
- [x] `GlobalCargoSourceID` provenance tracking across Industry, Town, and station sources.
- [x] `GlobalOrderDestinationID` classification across Station, Waypoint, Depot, and PortalGate destinations.
- [x] Consist Snapshot v2 encode/decode round-trip verifies exact reproduction of company, cargo provenance, and order lists.
- [x] Consist Snapshot v1 backward compatibility test confirms clean decoding of legacy snapshots.
- [x] Savegame `FIDS` round-trip test verifies full persistence and restoration of namespace, counters, and mappings.
- [x] Full Catch2 unit test suite passes: 168 test cases, 14,359 assertions.
- [x] Full CTest suite passes: 172/172 tests (100% pass rate).
- [x] Clean compilation for both `openttd` and `openttd_test` binaries with 0 errors and 0 warnings.
