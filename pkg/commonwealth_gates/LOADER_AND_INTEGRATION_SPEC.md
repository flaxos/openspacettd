# Commonwealth Wormhole Gate Family — Loader and Integration Specification

Status: **GRM ROUTE CHOSEN — PREPARATION STAGE CHECKPOINT**
The static NewGRF GRM reservation + Action A packaging route has been chosen and prepared in source.
C++ engine runtime loader/resolver hooks, tests, and rendering draw hooks remain **NOT IMPLEMENTED**.

---

## 1. Selected Engine Loading Mechanism: Static NewGRF GRM Reservation + Action A

Following source review of OpenSpaceTTD's NewGRF subsystem, the gate pack uses standard
GRM (General Resource Management) sprite reservation coupled with Action A:

1. **GRM Sprite Reservation (`src/newgrf/newgrf_actd.cpp`):**
   - Action D `ParamSet` `GlobalVar` (`GrfSpecFeature::GlobalVar`) with operation `op = 0` (`Reserve`)
     allocates 16 contiguous sprite slots from `_cur_gps.spriteid`.
   - During `GrfLoadingStage::Activation`, operation 0 returns the pre-allocated base ID into `param[10]`.
2. **Action A Range Validation (`src/newgrf/newgrf_acta.cpp`):**
   - `IsGRMReservedSprite()` verifies that the requested replacement range (`param[10]` to `param[10] + 16`)
     is owned by the current NewGRF's reservation, permitting Action A above `SPR_OPENTTD_BASE`.
   - Bypasses native tunnel IDs (2365..2372) entirely, preserving conventional mountain rail tunnels.
3. **NML Container Contract (`pkg/commonwealth_gates/commonwealth_gates.nml`):**
   - `param[10] = reserve_sprites(16);`
   - `replace gate_set(param[10], "sprites/gate_8bpp.png") { ... }`
   - `alternative_sprites(gate_set, ZOOM_LEVEL_NORMAL, BIT_DEPTH_32BPP, ...)`
   - `alternative_sprites(gate_set, ZOOM_LEVEL_IN_2X, BIT_DEPTH_32BPP, ...)`
4. **Runtime Access Contract:**
   - At runtime, the allocated base ID will be queried via `GetFileByGRFID(COMMONWEALTH_GATES_GRFID)->GetParam(10)`.
   - `_grm_sprites` is temporary and cleared after load; it must never be accessed at runtime.

---

## 2. Implementation Status & Exact Remaining Boundary

### Implemented in Current Preparation Stage:
- **Original Source Geometry:** Parametric hexagonal containment frame with centered static horizon aperture (`pkg/commonwealth_gates/gates.json`).
- **Deterministic 16-Sprite Sheets:** Normal 32bpp, normal 8bpp native fallback, 2x 32bpp, and neutral masks (`pkg/commonwealth_gates/sprites/`).
- **NML & Language Packaging:** `pkg/commonwealth_gates/commonwealth_gates.nml` and `pkg/commonwealth_gates/lang/english.lng`.
- **Preparation Tooling:** `scripts/build_commonwealth_gate_grf.py --prepare-only` checks file existence, manifest 16-sprite count, and generates/verifies NML source; it does not validate LNG strings or sheet dimensions (dimensions, hashes, and palettes are validated separately by `scripts/test_commonwealth_gate_art.py`).

### NOT IMPLEMENTED in Current Stage:
- **C++ Runtime Optional Pack Loader & Resolver:** No C++ classes (`src/portal/gate_sprites.*`) or lifecycle registration hooks in `src/newgrf.cpp` / `src/gfxinit.cpp` have been created or modified.
- **C++ Resolver Unit Tests:** `src/tests/test_gate_sprites.cpp` is NOT implemented.
- **Binary GRF Compilation & Action Audit:** `openspacettd_gates_v1.grf` compilation and byte-level action auditing are NOT IMPLEMENTED due to unavailable pinned `nmlc==0.9.0`.
- **Render Hook & In-Game Draw Integration:** No draw hooks in `src/tunnelbridge_cmd.cpp:DrawTile_TunnelBridge`.
- **In-Game Clearance & Vehicle Occlusion Proof:** Actual in-game clearance, train sorting, signal visibility, and catenary coexistence remain **UNVERIFIED / PENDING**.

---

## 3. Required Next Steps for C++ Loader Stage

When moving to the runtime integration stage:
1. Implement a minimal, reload-safe resolver `PortalGateSprites`:
   - Reset `base = 0` during `GfxLoadSprites()` (`src/gfxinit.cpp:334`) and `ResetNewGRFData()` (`src/newgrf.cpp:416`).
   - Query `GetFileByGRFID` after `AfterLoadGRFs()` in `src/newgrf.cpp`.
   - Validate all 16 sprites (normal dimensions, valid origin). Fail entire pack if partial or malformed.
2. In `DrawTile_TunnelBridge` (`src/tunnelbridge_cmd.cpp`):
   - Apply `to_underlying(direction) * 2` exactly once to the gate sprite base.
   - Suppress `railtype_overlay` portal arches while preserving rail surface overlays and catenary wires.
   - For `TransitRipple` and `QuarantineRed`, fall back to native state-paletted tunnel sprites until dedicated gate variants are provided.
