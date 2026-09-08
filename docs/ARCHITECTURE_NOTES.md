# OpenSpaceTTD Source-Code Architecture Map

This document maps the primary subsystems of OpenTTD relevant to OpenSpaceTTD development (multi-world logistics, planetary wormholes, high-throughput rail networks).

---

## 1. Map & Tile Handling

OpenTTD represents the map as a flat, power-of-two 2D grid up to 4096×4096 tiles. Each tile is indexed by a 32-bit `TileIndex`.

- **[src/tile_type.h](file:///home/flax/games/openspacettd/src/tile_type.h)**:
  - `TileIndex`: Strong typedef wrapper (`uint32_t`) representing tile IDs.
  - `TileType`: Enum defining tile classification (`Clear`, `Railway`, `Road`, `House`, `Trees`, `Station`, `Water`, `Void`, `Industry`, `TunnelBridge`, `Object`).
  - Spatial constants: `TILE_SIZE` (16 units), `TILE_HEIGHT` (8 units), `MAX_MAP_SIZE` (4096).
- **[src/map_type.h](file:///home/flax/games/openspacettd/src/map_type.h)**:
  - `TileIndexDiff` and `TileIndexDiffC`: Offsets and delta pairs for tile math.
  - `MIN_MAP_SIZE_BITS` (6 = 64) and `MAX_MAP_SIZE_BITS` (12 = 4096).
- **[src/map_func.h](file:///home/flax/games/openspacettd/src/map_func.h)**:
  - `Tile`: Wrapper providing structured access to map memory.
  - `TileBase` (8 bytes per tile: `type`, `height`, `m1`..`m5`) and `TileExtended` (4 bytes per tile: `m6`..`m8`).
  - Coordinate conversions: `TileX(t)`, `TileY(t)`, `TileXY(x, y)`, `TileDiffXY(x, y)`.
- **[src/map.cpp](file:///home/flax/games/openspacettd/src/map.cpp)**:
  - Memory allocation (`AllocateMap`), resizing, and boundary clearing.
- **[src/tile_map.h](file:///home/flax/games/openspacettd/src/tile_map.h) / [src/tile_map.cpp](file:///home/flax/games/openspacettd/src/tile_map.cpp)**:
  - Height query (`GetTileHeight`), slope detection (`GetTileSlope`), and terraforming operations.
- **[src/tilearea_type.h](file:///home/flax/games/openspacettd/src/tilearea_type.h) / [src/tilearea.cpp](file:///home/flax/games/openspacettd/src/tilearea.cpp)**:
  - `TileArea`: Rectangular tile iterators used for stations, industry footprints, and region bounds.

---

## 2. Trains

- **[src/train.h](file:///home/flax/games/openspacettd/src/train.h)**:
  - `Train` struct extending `GroundVehicle<Vehicle, VehicleType::Train>`.
  - Consist chaining (`first`, `next`, `last`), traction, power, acceleration curves, and braking physics.
  - Track occupation flags and collision statuses.
- **[src/train_cmd.cpp](file:///home/flax/games/openspacettd/src/train_cmd.cpp)**:
  - `TrainController`: Primary per-tick movement algorithm advancing train bogies across sub-tile positions.
  - `VehicleEnterTile_Train`: Validates track connections, triggers signals, and switches tile states.
  - Reversal, crash/derailment handling, station docking, and depot maintenance.
- **[src/train_gui.cpp](file:///home/flax/games/openspacettd/src/train_gui.cpp)**:
  - Train management GUIs, depot windows, orders interface, and train details display.
- **[src/vehicle.h](file:///home/flax/games/openspacettd/src/vehicle.h) / [src/vehicle.cpp](file:///home/flax/games/openspacettd/src/vehicle.cpp)**:
  - Base `Vehicle` pool allocator, z-position handling, and vehicle lifecycle.

---

## 3. Rail & Pathfinding

- **[src/rail.h](file:///home/flax/games/openspacettd/src/rail.h) / [src/rail.cpp](file:///home/flax/games/openspacettd/src/rail.cpp)**:
  - `Track` and `TrackBits` definitions (Track 0..5, plus special pseudo-tracks).
  - Track reservations and compatibility logic across rail types (standard, electrified, monorail, maglev).
- **[src/rail_cmd.cpp](file:///home/flax/games/openspacettd/src/rail_cmd.cpp)**:
  - Network construction commands (`CmdBuildSingleRail`, `CmdRemoveSingleRail`, `CmdBuildTrainDepot`).
- **[src/rail_map.h](file:///home/flax/games/openspacettd/src/rail_map.h)**:
  - Rail tile encoding in `TileBase` bits (`GetTrackBits`, `SetTrackBits`, signals layout).
- **[src/pbs.h](file:///home/flax/games/openspacettd/src/pbs.h) / [src/pbs.cpp](file:///home/flax/games/openspacettd/src/pbs.cpp)**:
  - Path-Based Signalling (PBS): `TryReserveRailPath`, path lookaheads, safe waiting points.
- **[src/pathfinder/yapf/](file:///home/flax/games/openspacettd/src/pathfinder/yapf/)**:
  - `yapf_rail.cpp`: Main train pathfinding entry point (`YapfTrainChooseTrack`).
  - `yapf_costrail.hpp`: Segment cost calculations, signal penalties, red-signal lookahead.
  - `yapf_destrail.hpp`: Goal detection for stations, waypoints, and depots.
  - `yapf_node_rail.hpp`: A* search node structures for rail graph exploration.
  - `follow_track.hpp`: Track follower stepping through trackbits across adjacent tiles.

---

## 4. Tunnels, Bridges & Wormholes

OpenTTD models tunnels and bridges as **wormholes**: vehicles entering a tunnel entrance enter a virtual state where they no longer occupy intermediate physical tiles on the grid.

- **[src/track_type.h](file:///home/flax/games/openspacettd/src/track_type.h)**:
  - `Track::Wormhole = 6`: Special flag indicating a vehicle is currently traversing a tunnel or bridge rather than an ordinary tile track.
- **[src/tile_cmd.h](file:///home/flax/games/openspacettd/src/tile_cmd.h)**:
  - `VehicleEnterTileState::EnteredWormhole`: State return signaling entrance into a wormhole.
- **[src/tunnelbridge_cmd.cpp](file:///home/flax/games/openspacettd/src/tunnelbridge_cmd.cpp)**:
  - `VehicleEnterTile_TunnelBridge`: Handles vehicles transitioning into `Track::Wormhole`, assigns entrance trajectory, and handles exit emergence.
  - `CmdBuildTunnel` and `CmdBuildBridge`: Placement validation and construction logic.
- **[src/tunnelbridge_map.h](file:///home/flax/games/openspacettd/src/tunnelbridge_map.h)**:
  - `GetOtherTunnelBridgeEnd(tile)`: Resolves the corresponding exit tile of a wormhole.
  - `GetTunnelBridgeDirection(tile)`: Retrieves entry orientation.
- **[src/tunnel_map.h](file:///home/flax/games/openspacettd/src/tunnel_map.h) / [src/tunnel_map.cpp](file:///home/flax/games/openspacettd/src/tunnel_map.cpp)**:
  - `GetOtherTunnelEnd`: Raycasts along the tunnel direction axis to find the opposing entrance tile at matching height.
  - `MakeRailTunnel`: Encodes tunnel entrance metadata onto the tile.

> **Relevance to OpenSpaceTTD Portals:**
> Inter-world railway wormholes directly leverage OpenTTD's existing `Track::Wormhole` vehicle state and extend `GetOtherTunnelBridgeEnd` via `PortalRegistry` in [src/portal/](file:///home/flax/games/openspacettd/src/portal/) to connect distant coordinates across logical map regions without requiring physical intermediate track tiles. See [docs/PORTAL_WORMHOLE_SPIKE.md](file:///home/flax/games/openspacettd/docs/PORTAL_WORMHOLE_SPIKE.md) for full architecture analysis and spike results.

---

## 5. Industries & Cargo

- **[src/industry.h](file:///home/flax/games/openspacettd/src/industry.h)**:
  - `Industry` pool struct: Coordinates, production rates, accepted/produced cargo types, cargo storage.
- **[src/industry_type.h](file:///home/flax/games/openspacettd/src/industry_type.h)**:
  - Industry specifications, production formulas, sound triggers, and NewGRF override tables.
- **[src/industry_cmd.cpp](file:///home/flax/games/openspacettd/src/industry_cmd.cpp)**:
  - `IndustryMonthlyLoop` and `IndustryDailyLoop`: Industry production updates, cargo generation, resource consumption, and closure logic.
- **[src/industry_map.h](file:///home/flax/games/openspacettd/src/industry_map.h)**:
  - Encodes industry indices and tile layouts inside `TileBase`.
- **[src/cargo_type.h](file:///home/flax/games/openspacettd/src/cargo_type.h) / [src/cargotype.h](file:///home/flax/games/openspacettd/src/cargotype.h)**:
  - `CargoType` and `CargoID` definitions, cargo classes (bulk, liquid, piece, passenger), freight transit times, and delivery payment rates.
- **[src/cargopacket.h](file:///home/flax/games/openspacettd/src/cargopacket.h) / [src/cargopacket.cpp](file:///home/flax/games/openspacettd/src/cargopacket.cpp)**:
  - `CargoPacket` and `CargoList`: Granular units of cargo tagged with origin station, loaded days, and payment tracing.
- **[src/linkgraph/](file:///home/flax/games/openspacettd/src/linkgraph/)**:
  - Cargo distribution (cargodist) routing graphs (`linkgraph.cpp`, `linkgraphjob.cpp`, `demands.cpp`).

---

## 6. Save & Load Subsystem

- **[src/saveload/saveload.h](file:///home/flax/games/openspacettd/src/saveload/saveload.h)**:
  - Savegame version enum (`SaveLoadVersion`), chunk tags (`CHUNK_MAPS`, `CHUNK_VEHS`, `CHUNK_INDY`, etc.), and handler registration macros.
- **[src/saveload/saveload.cpp](file:///home/flax/games/openspacettd/src/saveload/saveload.cpp)**:
  - Savegame file serialization and deserialization drivers (LZMA, LZO, ZLIB).
- **[src/saveload/map_sl.cpp](file:///home/flax/games/openspacettd/src/saveload/map_sl.cpp)**:
  - Serializes `TileBase` and `TileExtended` map arrays with RLE compression.
- **[src/saveload/vehicle_sl.cpp](file:///home/flax/games/openspacettd/src/saveload/vehicle_sl.cpp)**:
  - Serializes train, vehicle, and cargo packet states, restoring wormhole linkages.
- **[src/saveload/industry_sl.cpp](file:///home/flax/games/openspacettd/src/saveload/industry_sl.cpp)**:
  - Persists industries, production stages, stockpiles, and history counters.
- **[src/saveload/afterload.cpp](file:///home/flax/games/openspacettd/src/saveload/afterload.cpp)**:
  - Post-load validation, backward compatibility adjustments, and index recalculation.

---

## 7. Simulation Ticks & Timers

- **[src/openttd.cpp](file:///home/flax/games/openspacettd/src/openttd.cpp)**:
  - `StateGameLoop`: Central tick execution loop. Executes every game tick (~30 ms) unless paused.
    - `AnimateAnimatedTiles`: Visual tile cycling.
    - `CallVehicleTicks`: Advances vehicle physics, sub-tile stepping, signals, and collisions.
    - `CallLandscapeTick`: Natural vegetation growth, snow lines, and water events.
    - `RunTileLoop`: Random tile update cycles across the map grid.
    - Calendar / economy day increments when timer thresholds elapse.
- **[src/vehicle.cpp](file:///home/flax/games/openspacettd/src/vehicle.cpp)**:
  - `CallVehicleTicks`: Iterates through all allocated vehicles in the pool and calls `v->Tick()`.
- **[src/timer/](file:///home/flax/games/openspacettd/src/timer/)**:
  - `timer_game_tick.h`: Sub-day simulation tick counter (approx. 33.33 ticks per second).
  - `timer_game_calendar.h`: Calendar time advancement.
  - `timer_game_economy.h`: Economic tick accounting (financial periods, maintenance, inflation).

---

## 8. GUI & Viewports

- **[src/viewport.cpp](file:///home/flax/games/openspacettd/src/viewport.cpp) / [src/viewport_func.h](file:///home/flax/games/openspacettd/src/viewport_func.h)**:
  - Coordinate transformations: `RemapCoords` converts 3D tile/world coordinates `(x, y, z)` to 2D screen coordinates `(xp, yp)`.
  - Tile and vehicle sprite rendering pipeline, clipping rects, dirty region invalidations.
  - `viewport_kdtree.h`: Spatial index for fast sprite visibility queries.
- **[src/viewport_gui.cpp](file:///home/flax/games/openspacettd/src/viewport_gui.cpp)**:
  - Viewport interaction: mouse dragging, tile selection cursors, zoom levels, center-on-tile navigation.
- **[src/window.cpp](file:///home/flax/games/openspacettd/src/window.cpp) / [src/window_func.h](file:///home/flax/games/openspacettd/src/window_func.h)**:
  - Window hierarchy, widget trees, z-ordering, event handling (`OnTick`, `OnClick`, `OnPaint`, `OnDragDrop`).
- **[src/smallmap_gui.cpp](file:///home/flax/games/openspacettd/src/smallmap_gui.cpp)**:
  - Minimap drawing and filtering (industry view, transport routes, vegetation, owner overlays).
