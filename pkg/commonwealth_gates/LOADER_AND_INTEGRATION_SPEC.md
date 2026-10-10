# Commonwealth Wormhole Gate Family — Loader and Integration Specification

Status: **IMPLEMENTED & VERIFIED — RUNTIME INTEGRATION COMPLETE**
The static NewGRF GRM reservation + Action A packaging route is fully implemented in C++ runtime loader,
resolver hooks, Catch2 unit tests, and rendering draw hooks in `DrawTile_TunnelBridge`.

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
   - At runtime, the allocated base ID is queried via `GetFileByGRFID(COMMONWEALTH_GATES_GRFID)->GetParam(10)`.
   - `_grm_sprites` is temporary and cleared after load; it is never accessed at runtime.

---

## 2. Implementation Status & Verification Evidence

### Implemented & Verified in Runtime Integration:
- **Original Source Geometry:** Parametric hexagonal containment frame with centered static horizon aperture (`pkg/commonwealth_gates/gates.json`).
- **Deterministic 16-Sprite Sheets:** Normal 32bpp, normal 8bpp native fallback, 2x 32bpp, and neutral masks (`pkg/commonwealth_gates/sprites/`).
- **NML & Language Packaging:** `pkg/commonwealth_gates/commonwealth_gates.nml` and `pkg/commonwealth_gates/lang/english.lng`.
- **Delivered Binary NewGRF & Audit:** `bin/newgrf/openspacettd_gates_v1.grf` compiled with `nmlc==0.9.0`, validated with semantic binary container audit and 20/20 synthetic negative test cases (`scripts/build_commonwealth_gate_grf.py`).
- **CMake & Build Staging:** Staged into `${CMAKE_BINARY_DIR}/newgrf/` via `bin/newgrf/CMakeLists.txt` and registered in `demo/integrated_economy.cfg`.
- **C++ Runtime Resolver (`src/portal/gate_sprites.*`):**
  - Reset hook in `ResetNewGRFData()` (`src/newgrf.cpp`) and `GfxLoadSprites()` (`src/gfxinit.cpp`).
  - Active detection and base sprite validation in `AfterLoadGRFs()` (`src/newgrf.cpp`).
  - Safe bounds-checking and directional offset resolution (`_base + variant_offset + dir * 2 + part`).
- **Render Hook & In-Game Draw Integration (`src/tunnelbridge_cmd.cpp`):**
  - Custom rear aperture collar ground sprite drawn via `DrawGroundSprite(custom_gate_rear, PAL_NONE)`.
  - Custom front overhead containment arch added as sortable sprite via `AddSortableSpriteToDraw(custom_gate_front, PAL_NONE, *ti, roof_bounds[dir], false)`.
  - Suppresses `railtype_overlay` arches while preserving surface track overlays and catenary wires.
  - Fallback to native tunnel portals with visual overhaul excitation palettes when state is `TransitRipple` or `QuarantineRed`.
- **Automated Tests:** Catch2 test suite in `src/tests/test_gate_sprites.cpp` passing 65 assertions covering reset lifecycle, directional index calculations, unlinked standby amber excitation, linked active cyan pulse excitation, and transit ripple fallback.
