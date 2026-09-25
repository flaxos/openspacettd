# OpenSpaceTTD Current Architecture

**Source-verified snapshot at HEAD `a6cf83e6a6`** (branch `feature/sprint-50-gateway-staging-and-charters`)
**Date:** 2026-09-22
**Main branch HEAD:** `e4baa35623` (Sprints 49–50 are on feature branches, not yet merged to `main`)

This document records what the source code actually implements. It is authoritative
over historical sprint docs and roadmap claims. When this document conflicts with
source, update this document.

---

## 1. Single Global Map with Logical World Regions

OpenSpaceTTD uses **one global OpenTTD tile map** (up to 4096×4096). There is no
multi-map engine, no independent world loading/unloading, and no world-local
simulation loop. All worlds exist simultaneously as rectangular regions on the
single contiguous tile grid, separated by unbuildable void.

### Key implementation

| Component | File | Purpose |
|---|---|---|
| `PlanetRegion` | `src/portal/planet_manager.h` | Struct: `min_x/max_x/min_y/max_y`, name, `WorldPhase`, `WorldBiome`, `WorldID` |
| `PlanetManager` | `src/portal/planet_manager.h/.cpp` | Static manager: world registration, O(1) tile→world lookup via 64×64 spatial grid, construction placement validation, world iteration |
| `WorldID` | `src/portal/planet_type.h` | Strong typedef identifying a world region. Special values: `INVALID_WORLD`, `MIXED_WORLD` |
| Void enforcement | `PlanetManager::CheckConstructionPlacement` | Blocks building outside registered world bounds (`STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE`) |
| World generation | `src/portal/world_gen.h/.cpp` | Multi-world map generation with void isolation and biome-specific terrain |

### What would force true multi-map?

No current gameplay requirement demands independent maps. The single-map approach
breaks down only if:
- Total world area exceeds 4096×4096 tiles (currently ~6 worlds fit comfortably)
- Independent world simulation rates are needed (e.g. background/LOD worlds)
- Memory pressure requires unloading inactive worlds
- World-local save/load is needed for federation handoff

None of these are current requirements. The 108-world universe graph is metadata
for trade simulation, not a demand for 108 simultaneously loaded map regions.

---

## 2. Portal / Wormhole System

Portals reuse OpenTTD's tunnel/bridge wormhole mechanics. A vehicle entering a
portal transitions to `Track::Wormhole` state and emerges at a distant tile in
another world region, bypassing all intermediate tiles.

### Key implementation

| Component | File | Purpose |
|---|---|---|
| `PortalRegistry` | `src/portal/portal_registry.h/.cpp` | Hash-map storage of `PortalLink` records (tile→portal, portal→link pair) |
| `PortalTerminal` | `src/portal/portal_terminal.h/.cpp` | Multi-platform terminal approach/departure infrastructure |
| Portal commands | `src/portal/portal_cmd.h/.cpp` | Construction, linking and demolition commands |
| YAPF heuristic | `src/pathfinder/yapf/yapf_rail_portal_heuristic.hpp` | Portal shortcuts in A* pathfinding cost estimates |
| Train traversal | `src/train_cmd.cpp` (~L101) | Bypasses Euclidean checks when `w->track == Track::Wormhole` |
| Portal types | `src/portal/portal_type.h` | Portal classification and operational states |

### Portal routing reality

- **A→B single hop:** Implemented and tested. Trains route through one portal between two worlds.
- **A→B→C multi-hop:** Pathfinder accounts for portal shortcuts. YAPF can plan routes through multiple portals. The `ConsistMaterializer` assigns round-trip orders across worlds. However, **end-to-end multi-hop with branching topologies has limited direct test coverage**.
- **Capacity/congestion:** Sprint 50 adds multi-track throat signalling and staging sidings. Congestion-based holding loops are implemented. Economic consequences of congestion are not yet proven in gameplay.
- **Reliability:** Portal operational state persistence exists. Recovery after save/load during transit is handled but has limited black-box test coverage.

### Save/load chunks for portals

| Chunk | Content |
|---|---|
| `PLNT` | Planet regions (worlds) |
| `PORT` | Portal links |
| `PRTX` | Portal terminal extensions |

---

## 3. Economy and Production Systems

Multiple overlapping economy systems exist. Their integration is a known architectural concern.

### Standard OpenTTD economy
- Cargo delivery revenue, company finances, industry production loops
- Runs via `economy.cpp`, `industry_cmd.cpp`, station cargo handling

### Commonwealth overlay systems

| Manager | File | Purpose | Chunk |
|---|---|---|---|
| `CompanyStockpile` | `company_stockpile.h/.cpp` | Per-company, per-world commodity stockpiles | `STCK` |
| `LogisticsHub` | `logistics_hub.h/.cpp` | Bi-directional buffering with reserve floors | `LHUB` |
| `FabricationManager` | `fabrication_manager.h/.cpp` | BOM-based construction cost (stockpile deduction with cash discount) | `FABR` |
| `TechTreeManager` | `tech_tree.h/.cpp` | R&D progression, technology unlocks, fabrication bonuses | `TECH` |
| `ProductionChainManager` | `production_chain.h/.cpp` | 12-cargo four-pipeline factory conversion simulation | `PROD` |
| `CorporateHQManager` | `corporate_hq.h/.cpp` | HQ placement (Phase 1 Core only), tier advancement | `CHQS` |
| `MegacityManager` | `megacity_manager.h/.cpp` | Megacity demand, supply integration, population growth | `MEGA` |
| `EdgeConduit` | `edge_conduit.h/.cpp` | Off-world trade via spaceports | `COND` |
| `PrebuiltTradeManager` | `prebuilt_trade.h/.cpp` | 108-world trade simulation proxy (tariffs, scheduled returns) | `TRAD` |
| `CommonwealthPackManager` | `commonwealth_pack.h/.cpp` | NewGRF industry/cargo pack integration | — |

### CommonwealthCargoID vs CargoType

`CommonwealthCargoID` is a static 13-item enum of space-industrial goods (IronOre,
SiliconChips, Superalloys, etc.). `ProductionChainManager` bridges these to dynamic
OpenTTD `CargoType` values at runtime. The two systems are loosely coupled — the
Commonwealth overlay does not replace the standard cargo system but adds parallel
accounting.

### Economy ownership question

Supply/demand is currently driven by:
1. Standard OpenTTD industry specs (production rates, acceptance)
2. `ProductionChainManager` monthly conversion (inputs→outputs)
3. `MegacityManager` demand quotas
4. `PrebuiltTradeManager` off-world trade tariffs
5. `EdgeConduit` spaceport revenue

These are **not unified** — they are layered overlays. A coherent world economy
where inter-world transport is economically necessary is a stated goal but not
yet proven by gameplay.

---

## 4. Federation System

### Key implementation

| Component | File | Purpose | Chunk |
|---|---|---|---|
| `FederationIdentity` | `federation_identity.h/.cpp` | Global player/company identifiers | `FIDS` |
| `ConsistSnapshot` | `consist_snapshot.h/.cpp` | Serialised train state for cross-server transfer | — |
| `ConsistMaterializer` | `consist_materializer.h/.cpp` | Reconstruct trains from snapshots | — |
| `AuthorityTransport` | `authority_transport.h/.cpp` | HTTP-based state sync with Universe Authority server | — |
| `TransferJournal` | `transfer_journal.h/.cpp` | Cross-server transfer records and checkpoints | `FJRN` |
| `UniverseAuthority` | `universe_authority.h/.cpp` | Central coordination service | — |
| `FederationStaging` | `federation_staging.h/.cpp` | Train staging for cross-server transit | — |
| Cluster testbed | `scripts/cluster_testbed.py` | Multi-node test harness | — |

### Federation reality assessment

- **Domain model and protocol:** Fully implemented with automated tests (identity,
  snapshot serialisation, materialisation, transfer journal).
- **Cross-process transport:** Works via manual `federation_dispatch` console
  command. Natural gate-entry departure between independent game processes is
  **not proven**.
- **Cluster testbed:** `scripts/cluster_testbed.py` runs 3-node smoke tests.
  These verify protocol correctness, not natural gameplay.
- **Live multiplayer federation gameplay:** Not demonstrated. Federation is
  currently infrastructure, not a player-facing feature.

---

## 5. Corporate and Alliance Systems

| Manager | File | Chunk | Status |
|---|---|---|---|
| `CorporateAllianceManager` | `corporate_alliance.h/.cpp` | `ALLI` | Implemented: Hostile/Neutral/Allied relations, trackage rights enforcement |
| `CorporateCharter` | `corporate_charter.h/.cpp` | — | Sprint 50: gate access policies, toll collection |
| `LoreCompetitor` | `lore_competitor.h/.cpp` | `LORE` | AI competitors (CST, Grand Central) with scripted networks |

---

## 6. Blueprint System

| Component | File | Purpose |
|---|---|---|
| `Blueprint` | `src/blueprint/blueprint.h/.cpp` | JSON-based portable rail layout definition |
| `CmdPlaceBlueprint` | `src/blueprint/blueprint_cmd.h/.cpp` | Server-authoritative multi-tile placement with BOM consumption |
| `BlueprintManager` | `src/blueprint/blueprint_manager.h/.cpp` | Storage, import/export, library management |
| `BlueprintGUI` | `src/blueprint/blueprint_gui.h/.cpp` | Capture, preview and placement UI |

Blueprint placement was a reported crash (WP-01). The crash was repaired in recovery;
graphical acceptance is recorded as pending.

---

## 7. Universe Graph

| Component | File | Purpose | Chunk |
|---|---|---|---|
| `UniverseGraphManager` | `universe_graph.h/.cpp` | 108-world canonical topology from Hamilton lore | — |
| `PrebuiltTradeManager` | `prebuilt_trade.h/.cpp` | Gateway tariff simulation without live external servers | `TRAD` |

The 108-world graph is **metadata** driving trade simulation and valid-connection
constraints. It does **not** mean 108 simultaneously loaded map regions.

---

---

## 8. World Progression: Placement Restrictions vs Simulation Progression

A central architectural question is: **What in world progression is static metadata/placement restriction versus true simulation-driven progression?**

### Static Metadata & Placement Restrictions
- **WorldPhase Classification:** Each `PlanetRegion` has a `WorldPhase` (Phase 1 Core, Phase 2 Industrial, Phase 3 Extraction/Frontier, Phase 4 Wilderness). This classification is assigned at world generation time.
- **Construction Restrictions:** `PlanetManager::CheckConstructionPlacement` enforces:
  * No building in void space between worlds (`STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE`).
  * Corporate HQ can *only* be founded on Phase 1 Core worlds.
  * Heavy industrial processing is prohibited on Phase 4 Wilderness worlds.
  * Raw extraction facilities are locked to Phase 3/4 worlds.
- **Technology Gating:** `TechTreeManager` gates rolling stock and fabrication recipes behind research nodes (e.g. `TECH_TRACTION_1..4`), but this is a capability gate rather than an automatic map-morphing simulation.

### True Simulation-Driven Progression
- **Megacity Consumption & Growth:** `MegacityManager` runs a monthly simulation tick checking cargo delivery against population demand quotas. If quotas are met, population score increases and arcologies can advance tiers.
- **Phase Promotion Events:** In Sprint 47, Phase 4 Wilderness worlds can be promoted to Phase 3 Emerging worlds upon completion of 3-tier megaproject supply delivery quotas (life-support, structural steel, power conduits).
- **Town Growth Interception:** Town growth in `megacity_manager.cpp` is throttled if off-world commodity supply loops fall below minimum thresholds.

---

## 9. Visual / Content Systems

| Component | File | Chunk |
|---|---|---|
| `VisualOverhaul` | `visual_overhaul.h/.cpp` | `VISU` |
| `CommonwealthPack` | `commonwealth_pack.h/.cpp` | — |
| NewGRF packs | `pkg/commonwealth_industry/`, `pkg/commonwealth_rail/`, `pkg/commonwealth_visuals/` | — |

Three in-tree NML packs exist (`OST\x01` industry, `OST\x02` rolling stock,
`OST\x03` visuals). Sprint 38 bespoke original art remains planned.

---

## 10. Complete Save/Load Chunk Registry & Persistence Validation

All custom chunks are registered in `src/saveload/planet_sl.cpp` and processed in order:

| Chunk | Type | System / Manager | Save/Load Handlers | Round-Trip Test Coverage |
|---|---|---|---|---|
| `PLNT` | Table | `PlanetManager` (worlds & regions) | `Save_PLNT` / `Load_PLNT` | ✅ Covered in `test_saveload_planet.cpp` |
| `PORT` | Table | `PortalRegistry` (portal pairs & links) | `Save_PORT` / `Load_PORT` | ✅ Covered in `test_saveload_planet.cpp`, `test_portal_construction.cpp` |
| `PRTX` | Table | `PortalTerminal` (extended terminal geometry) | `Save_PRTX` / `Load_PRTX` | ✅ Covered in `test_portal_construction.cpp` |
| `FIDS` | Table | `FederationIdentity` (global company/consist IDs) | `Save_FIDS` / `Load_FIDS` | ✅ Covered in `test_federation.cpp` |
| `SPRT` | Table | `SpaceportManager` (orbital spaceport hubs) | `Save_SPRT` / `Load_SPRT` | ✅ Covered in `test_spaceports_and_conduits.cpp` |
| `COND` | Table | `EdgeConduit` (virtual off-world conduits) | `Save_COND` / `Load_COND` | ✅ Covered in `test_spaceports_and_conduits.cpp` |
| `MEGA` | Table | `MegacityManager` (megacity population & quotas) | `Save_MEGA` / `Load_MEGA` | ✅ Covered in `test_federation_megacity_economy.cpp` |
| `STCK` | Table | `CompanyStockpile` (per-world commodity balances) | `Save_STCK` / `Load_STCK` | ✅ Covered in `test_sprint39_corporate_hq_and_stockpile.cpp` |
| `LHUB` | Table | `LogisticsHub` (bi-directional buffering & floors) | `Save_LHUB` / `Load_LHUB` | ✅ Covered in `test_sprint39_corporate_hq_and_stockpile.cpp` |
| `CHQS` | Table | `CorporateHQManager` (campus tier & location) | `Save_CHQS` / `Load_CHQS` | ✅ Covered in `test_sprint39_corporate_hq_and_stockpile.cpp` |
| `FABR` | Table | `FabricationManager` (company dual-mode setting) | `Save_FABR` / `Load_FABR` | ✅ Covered in `test_sprint40_fabrication_engine.cpp` |
| `ALLI` | Table | `CorporateAllianceManager` (diplomatic relations) | `Save_ALLI` / `Load_ALLI` | ✅ Covered in `test_sprint47_alliances_and_neutral_tracks.cpp` |
| `FJRN` | Table | `TransferJournal` (cross-server transfer records) | `Save_FJRN` / `Load_FJRN` | ✅ Covered in `test_sprint46_live_federation.cpp` |
| `FTJR` | ReadOnly | Legacy transfer journal (migration chunk) | `Load_FTJR` only | ✅ Covered in legacy migration test |
| `ISPR` | Table | Industry spaceport bindings | `Save_ISPR` / `Load_ISPR` | ✅ Covered in `test_spaceports_and_conduits.cpp` |
| `TECH` | Table | `TechTreeManager` (research DAG progression) | `Save_TECH` / `Load_TECH` | ✅ Covered in `test_sprint41_tech_tree.cpp` |
| `PROD` | Table | `ProductionChainManager` (facility states) | `Save_PROD` / `Load_PROD` | ✅ Covered in `test_sprint42_production_chains.cpp` |
| `LORE` | Table | `LoreAIManager` (competitor scripts & expansion) | `Save_LORE` / `Load_LORE` | ✅ Covered in `test_sprint44_lore_ai.cpp` |
| `VISU` | Table | `VisualOverhaul` (arcology visual stages) | `Save_VISU` / `Load_VISU` | ✅ Covered in `test_sprint45_visual_overhaul.cpp` |
| `TRAD` | Table | `PrebuiltTradeManager` (gateway tariffs & returns) | `Save_TRAD` / `Load_TRAD` | ✅ Covered in `test_prebuilt_trade.cpp` |

**Post-load reference validation:** Chunks reconstruct pointers to `Station`, `Industry`, and `TileIndex` entities. Post-load verification is hooked into `afterload.cpp` to discard orphaned portal endpoints or invalid company references after map resize or savegame migration.

---

## 10. Modified OpenTTD Core Files

Key upstream files with OpenSpaceTTD hooks:

| File | Modification |
|---|---|
| `src/train_cmd.cpp` | Portal wormhole traversal bypass |
| `src/console_cmds.cpp` | ~40 custom console commands (federation, universe, commonwealth, UAT) |
| `src/economy.cpp` | Commonwealth monthly production/conduit hooks |
| `src/industry.h/.cpp` | Extended industry types for Commonwealth cargos |
| `src/vehicle.cpp` | Portal-aware vehicle tick processing |
| `src/map.cpp` | Extended map allocation for larger world partitions |
| `src/company_cmd.cpp` | Corporate HQ and stockpile integration |
| `src/pathfinder/yapf/yapf_destrail.hpp` | Portal heuristic integration |

All custom gameplay logic is concentrated in `src/portal/` (85 files, 44 headers)
and `src/blueprint/` (8 files). This is the correct isolation pattern.

---

## 11. Testing Infrastructure

- **Framework:** Catch2 (`src/3rdparty/catch2/`)
- **Test files:** 71 files in `src/tests/`
- **Registered CTest cases:** 435+ at HEAD (all passing)
- **Regression tests:** Squirrel AI, GameScript, stationlist, blueprint, window validation
- **CI:** Linux build with CTest + repo linters (`file-descriptions.py`, `unused-strings.py`)

## Connected economy acceptance adapter — 2026-09-23

`src/portal/connected_economy.cpp` adds an isolated offline scenario builder and
read-only audit interface. It uses existing logical regions, normal construction,
orders, timetables, production facilities, hubs and town growth. It introduces no
new persistent chunk or independent map. [Details](CONNECTED_ECONOMY_UAT.md).

Megacity demand now resolves loaded cargo labels. Stations serving a registered
megacity's actual houses accept its demand cargoes; only town-consumed remainder
counts toward monthly supply, excluding industry inputs and warehouse storage.
The monthly economy hook synchronizes quota population from the native town.
Industry v3 occupies cargo slots 16–28 with explicit freight flags; rail v3 retains
native food/grain support and adds a livestock stockcar. Exact v2 packs remain
supported for existing saves. Optional observer counters reconcile cargo flows and
research consumption without changing simulation behavior.

## Connected fixture recovery — 2026-09-23

`RepairConnectedEconomyTerrain` runs before post-load terrain use, restricted to
the marked connected demo. A deterministic corner-height relaxation first plans
continuous slopes, then validates all four adjacent tiles of every changed corner,
and only then writes the heights. It refuses to touch infrastructure. Generation
uses the same routine before constructing the network. No new save chunk or world
layout is introduced. `CommonwealthPackManager::RepairLegacyCargoStrings` repairs
only the identified v2/v3 industry pack after GRF string mapping. Industry v4
contains complete cargo text directly; legacy binaries retain their exact hashes.

## Draft federation reliability branch — 25 September 2026

The `fix/federation-scheduled-recovery` branch adds an asynchronous, bounded
request queue for multiplayer authority traffic. HTTP results feed native
replicated commands; singleplayer retains its existing transport path.
Foreign station destinations resolve through explicit full-identity mappings to
local gate booking stations. The adapter directs native station orders to the
interserver gate without stopping at the booking station.

Snapshot version 3 adds per-packet quantity, age, feeder share, payment vectors
and global provenance, plus native station order flags. Versions 1/2 remain
readable with their original aggregate-cargo limitations. New FSCH and FGCP
chunks persist full-identity master schedules and packet provenance; FIDS
preserves explicit foreign company/station aliases. These are draft changes:
nonempty new-chunk reload and complete scheduled recovery acceptance remain
unverified. Each server still owns one ordinary global tile map.

The checkpoint tool hashes a matched pair of saves, authority state and binary.
Its recovery contract accepts only the latest coordinated checkpoint, not
arbitrary historical saves or unexpected-crash recovery.
