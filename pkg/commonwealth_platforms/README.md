# Commonwealth platform art proof

An optional original art pack for the four low open platform sprites in the
native station family. Graphite seams, steel modular decks, pale ceramic edges,
small amber hazard ticks and cyan service marks establish the industrial palette.
Company colour occupies separate inset panels. The normal view governs readability.

The source and exporter are original OpenSpaceTTD work, copyright 2026
OpenSpaceTTD Developers, licensed **GPL-2.0-only**, with the full licence in
[COPYING.md](../../COPYING.md). No generated concept image or external artwork is
used as production pixels. The parametric source is
[platforms.json](platforms.json); edit it or the explicit world-space geometry in
the exporter, then rebuild. Any new third-party source must record its own author,
licence and provenance before incorporation.

## Build and verify

```sh
python3 -m pip install -r requirements-platform-art.txt
python3 scripts/build_commonwealth_platform_grf.py
python3 scripts/build_commonwealth_platform_grf.py --verify
```

The exporter pins NML 0.9.0 and Pillow 12.3.0. It renders each zoom directly from
the same model; no nearest-neighbour enlargement supplies the 2x art. It exports
DOS-palette 8bpp fallback, normal/2x RGBA sheets and matching indexed recolour masks.
Index 0 is transparent in the fallback and retains RGBA colour in the 32bpp mask.
Only primary company ramp index 201 appears in the mask. Ordinary materials avoid
company, secondary-company and animated palette ranges. There is no animation.
All outputs, full SHA-256 hashes, tool versions and engine content MD5 are recorded
in [manifest.json](manifest.json). Verification rebuilds privately and compares
every output, including the compiled GRF. A compiled action allowlist rejects
simulation actions or a replacement range other than IDs 1069–1072.

## Compatibility contract

| Surface | Contract |
|---|---|
| Identity | `OST\06`, version 1; numeric engine GRFID `0654534f`, textual config ID `4f535406` |
| Sprite scope | 1069 Y-front, 1070 X-rear, 1071 Y-rear, 1072 X-front, native Action A |
| Geometry | Original strip anchor, 16×5×2 or 5×16×2 world units; no tall objects |
| Projection | `x=2*(world_y-world_x)`, `y=world_x+world_y-world_z`, native 64×32 tile |
| Sheets | 44×25 normal cells / 88×50 2x cells; identical RGBA/mask dimensions and offsets |
| Offsets | X: −32,−2 normal / −64,−4 2x; Y: −10,−2 / −20,−4; no cropping |
| Content | Separate optional pack; original industry, rail, equipment and terrain assets unchanged |
| Simulation | No station items, callbacks, properties, costs, capacities, RNG or commands |
| Save | Use as engine-admitted static art before generating the isolated fixture; test cold reload/removal |
| Network | General and federated sessions require their own exact manifest test; no network exemption promised |

The replacements affect **every native station layout that references these
sprite IDs**, including open strips beside native buildings and shelters.
Other NewGRFs can replace the same IDs, so load order and base-set interactions
remain review requirements. 4x art, other native station sprites, custom station
families, world-dependent visuals, terrain, vehicle fleets and a UI theme are
outside this proof. Higher resolution does not increase the native sorting box.

## Isolated review

```sh
python3 scripts/test_commonwealth_platform_art.py --output build/platform-art-proof
python3 scripts/preview_commonwealth_platform_art.py
python3 scripts/preview_commonwealth_platform_art.py --baseline
```

Build the current engine first. The preview reuses the native WP11 fixture and
creates a disposable profile; it has authored capital and routes, so it is art
evidence only. Existing profiles and saves are preserved. Linux graphical QA
requires SDL, Xvfb, libX11 and libXtst. The preview itself needs a working desktop
and installed base graphics; platform-specific packaging remains separate.

Review both axes, open/mixed shelter layouts, train occlusion, company colours,
station transparency, normal/2x zoom and UI scale. Run one freight service, save,
quit and reload. Compare frame time and RSS in the same scene; a tiny paused scene
does not establish long-run or large-network performance. Broader replacement
starts only after human acceptance of this family and its production pipeline.

Official rendering references:
[replacement sprites](https://newgrf-specs.tt-wiki.net/wiki/NML:Replace_TTD_sprites),
[real sprites](https://newgrf-specs.tt-wiki.net/wiki/NML:Realsprites),
[alternative zooms](https://newgrf-specs.tt-wiki.net/wiki/NML:Alternative_sprites).
Native interfaces are in `src/table/station_land.h`, `src/sprite.cpp` and
`src/viewport.cpp`.
