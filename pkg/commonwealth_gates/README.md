# Commonwealth Wormhole Gate Family

An original industrial sci-fi asset family for rail portal gates in OpenSpaceTTD.
Engineered hexagonal containment frames, graphite structural members, pale ceramic
safety bezels, restrained cyan active emitter clusters and amber standby nodes
establish the Commonwealth wormhole aesthetic.

The source geometry and exporter are original OpenSpaceTTD work, copyright 2026
OpenSpaceTTD Developers, licensed **GPL-2.0-only** (see [COPYING.md](../../COPYING.md)).
No external, third-party, copyrighted or concept-generator images are used as
production pixels. The parametric model is defined in [gates.json](gates.json);
pixel assets are generated deterministically by `scripts/build_commonwealth_gate_art.py`.

A detailed loader and integration specification is documented in
[LOADER_AND_INTEGRATION_SPEC.md](LOADER_AND_INTEGRATION_SPEC.md).

## Sprite Architecture and Native Alignment

The asset family provides **16 distinct sprites** across two static state variants:
- **Active Cyan (Linked Gate):** Sprites 0..7 (4 directions × rear/front)
- **Standby Amber (Unlinked Gate):** Sprites 8..15 (4 directions × rear/front)

| Direction | Variant | Component | Index | Unified Offset (1x) | Description |
|---|---|---|---|---|---|
| NE (0) | Active | Rear / Ground | 0 | `[-24, -12]` | Hexagonal mounting collar, track ballast pass-through, cyan event horizon |
| NE (0) | Active | Front / Arch | 1 | `[-24, -12]` | Overhead hexagonal containment arch, ceramic bezel, cyan crown emitters |
| SE (1) | Active | Rear / Ground | 2 | `[-24, -12]` | Hexagonal mounting collar, track ballast pass-through, cyan event horizon |
| SE (1) | Active | Front / Arch | 3 | `[-24, -12]` | Overhead hexagonal containment arch, ceramic bezel, cyan crown emitters |
| SW (2) | Active | Rear / Ground | 4 | `[-24, -8]` | Hexagonal mounting collar, track ballast pass-through, cyan event horizon |
| SW (2) | Active | Front / Arch | 5 | `[-24, -8]` | Overhead hexagonal containment arch, ceramic bezel, cyan crown emitters |
| NW (3) | Active | Rear / Ground | 6 | `[-24, -8]` | Hexagonal mounting collar, track ballast pass-through, cyan event horizon |
| NW (3) | Active | Front / Arch | 7 | `[-24, -8]` | Overhead hexagonal containment arch, ceramic bezel, cyan crown emitters |
| NE (0) | Standby | Rear / Ground | 8 | `[-24, -12]` | Hexagonal mounting collar, track ballast pass-through, amber event horizon |
| NE (0) | Standby | Front / Arch | 9 | `[-24, -12]` | Overhead hexagonal containment arch, ceramic bezel, amber crown emitters |
| SE (1) | Standby | Rear / Ground | 10 | `[-24, -12]` | Hexagonal mounting collar, track ballast pass-through, amber event horizon |
| SE (1) | Standby | Front / Arch | 11 | `[-24, -12]` | Overhead hexagonal containment arch, ceramic bezel, amber crown emitters |
| SW (2) | Standby | Rear / Ground | 12 | `[-24, -8]` | Hexagonal mounting collar, track ballast pass-through, amber event horizon |
| SW (2) | Standby | Front / Arch | 13 | `[-24, -8]` | Overhead hexagonal containment arch, ceramic bezel, amber crown emitters |
| NW (3) | Standby | Rear / Ground | 14 | `[-24, -8]` | Hexagonal mounting collar, track ballast pass-through, amber event horizon |
| NW (3) | Standby | Front / Arch | 15 | `[-24, -8]` | Overhead hexagonal containment arch, ceramic bezel, amber crown emitters |

### Hexagonal Framing and Clear Envelope
- **Engineered Hexagonal Hoop:** The containment arch is constructed from six distinct faceted structural members forming a closed hexagonal hoop. The bottom sill rests on the ballast bed in the rear sprite, while the upper five facets form the front overhead arch.
- **Clearance Envelope:** The inner hexagonal opening provides an 18-pixel wide by 22-pixel high transparent portal envelope, completely clear of front arch pixels. Local transparent pixel envelope only; native train clearance/sorting unverified.
- **32bpp Blitter Semantics & Palette:** State cues (cyan active vs amber standby) are baked as explicit static sprite variants to ensure consistent appearance across blitters. Materials are defined as static engineered Commonwealth graphite, steel, and pale ceramic. `PAL_NONE` leaves company livery un-recoloured; therefore no company livery claims are made.

### Unverified In-Game Status
In-game vehicle occlusion, sorting bounds against moving trains, signal contrast, and
runtime rendering behavior are **UNVERIFIED / PENDING** native renderer integration.
No claim of completed in-game delivery is made at this asset stage.

## Loader Route & Implementation Status

- **Chosen Engine Packaging Route:** Static NewGRF GRM reservation + Action A:
  - `param[10] = reserve_sprites(16);` reserves 16 contiguous sprite IDs in Action D.
  - `replace gate_set(param[10], "sprites/gate_8bpp.png")` populates the 16 sprites.
  - `alternative_sprites` provides 32bpp normal and 2x zoom replacements.
  - Avoids overwriting native mountain tunnel IDs 2365..2372.
- **NML & Language Packaging:** Complete (`pkg/commonwealth_gates/commonwealth_gates.nml` and `pkg/commonwealth_gates/lang/english.lng`).
- **C++ Engine Loader & Resolver:** **NOT IMPLEMENTED** in this stage. No engine source files were modified.
- **Binary GRF Compilation & Action Audit:** **NOT IMPLEMENTED / BLOCKED** due to unavailable pinned `nmlc==0.9.0`.

## Build and Verification

```sh
# Generate sheets and manifest
python3 scripts/build_commonwealth_gate_art.py

# Verify byte determinism against current model
python3 scripts/build_commonwealth_gate_art.py --verify

# Run asset verification suite including composited pair proof
python3 scripts/test_commonwealth_gate_art.py

# Run NML and LNG packaging preparation check
python3 scripts/build_commonwealth_gate_grf.py --prepare-only
```

Generated outputs recorded in [manifest.json](manifest.json):
- `gate_32bpp.png`: 48×768 RGBA normal zoom sheet (16 sprites)
- `gate_mask.png`: 48×768 8bpp mask
- `gate_8bpp.png`: 48×768 8bpp native DOS palette fallback (index 0 transparent)
- `gate_2x_32bpp.png`: 96×1536 RGBA 2x zoom sheet (16 sprites)
- `gate_2x_mask.png`: 96×1536 8bpp mask
