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

## 8. Visual / Content Systems

| Component | File | Chunk |
|---|---|---|
| `VisualOverhaul` | `visual_overhaul.h/.cpp` | `VISU` |
| `CommonwealthPack` | `commonwealth_pack.h/.cpp` | — |
| NewGRF packs | `pkg/commonwealth_industry/`, `pkg/commonwealth_rail/`, `pkg/commonwealth_visuals/` | — |

Three in-tree NML packs exist (`OST\x01` industry, `OST\x02` rolling stock,
`OST\x03` visuals). Sprint 38 bespoke original art remains planned.

---

## 9. Complete Save/Load Chunk Registry

All custom chunks are defined in `src/saveload/planet_sl.cpp`:

| Chunk | Type | System |
|---|---|---|
| `PLNT` | Table | Planet regions |
| `PORT` | Table | Portal links |
| `PRTX` | Table | Portal terminal extensions |
| `FIDS` | Table | Federation identities |
| `SPRT` | Table | Spaceport records |
| `COND` | Table | Edge conduit entries |
| `MEGA` | Table | Megacity state |
| `STCK` | Table | Company stockpiles |
| `LHUB` | Table | Logistics hub state |
| `CHQS` | Table | Corporate HQ profiles |
| `FABR` | Table | Fabrication mode settings |
| `ALLI` | Table | Corporate alliance relations |
| `FJRN` | Table | Federation transfer journal |
| `FTJR` | ReadOnly | Legacy transfer journal (migration) |
| `ISPR` | Table | Industry spaceport bindings |
| `TECH` | Table | Tech tree state |
| `PROD` | Table | Production chain facilities |
| `LORE` | Table | Lore competitor state |
| `VISU` | Table | Visual overhaul arcology data |
| `TRAD` | Table | Prebuilt trade gateways |

**20 custom chunks** total. All have save handlers. Round-trip test coverage
varies — see sprint ledger for per-system evidence.

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
