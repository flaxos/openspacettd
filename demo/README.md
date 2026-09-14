# OpenSpaceTTD UAT Demo

> **Active Sprint 36 All-Feature Fixture:** `OpenSpaceTTD-All-Features-UAT-v1.0.sav` is the canonical guided solo UAT release savegame covering all features from Sprint 1 through Sprint 40. It features a 6-world procedural partition covering all 6 environmental biomes (Temperate Core, Arid Desert, Sub-Arctic, Volcanic, Sub-Tropic, Oceanic), 5 monumental gateway pairs, multi-tier megacity demand economics, planetary company stockpiles, corporate headquarters, logistics hubs, and in-kind fabrication. Powered by GameScript v8 with a 12-chapter, 25-goal persistent Story Book. See [ALL-FEATURES-UAT.md](ALL-FEATURES-UAT.md), [FEATURE_UI_UAT_COVERAGE.md](../docs/FEATURE_UI_UAT_COVERAGE.md), and [PROJECT_STATUS_AND_ROADMAP.md](../docs/PROJECT_STATUS_AND_ROADMAP.md).

> **Legacy Sprint 28 Fixture:** `OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav` remains preserved for regression testing and covers features through Sprint 27. See [SPRINT28-UAT.md](SPRINT28-UAT.md).

### Quick Launch: All-Features Scenario (v1.0)
```bash
./build/openttd -g demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav
```

---

## World Layout & Environmental Biomes

- **World 1 (Phase 1 Core):** Anchored by **Oaktree Core**, styled with Temperate Core biomes. Consumer and high-tech center.
- **World 2 (Phase 2 Developed):** Anchored by **Merredin Industrial**, styled with Arid Industrial biomes. Heavy processing backbone, CST prefab staging area, and custom blueprint capture layout.
- **World 3 (Phase 3 Frontier):** Anchored by **Calyx Frontier**, styled with Sub-Arctic Frontier biomes. Primary resource extraction and Edge Conduit void boundary.
- **Gateway Alpha:** Interlinks Worlds 1 and 2 with non-aligned, perpendicular portal heads and automatic 18-tile terminals. Its demonstrator locomotive continuously traverses between worlds.
- **Gateway Beta:** Interlinks Worlds 2 and 3 with rotated portal heads and automatic 18-tile terminals.

Use `Ctrl+Alt+1`, `Ctrl+Alt+2`, and `Ctrl+Alt+3` or the Map dropdown menu to jump between worlds.
The in-game Story Book contains 7 chapters with 16 persistent, measurable acceptance goals and clickable location pins.

## Dedicated UAT Testing Sites

1. **Phase 1 Spaceport Candidate:** Owned small airport in World 1 with Spaceport designation, Tier 1-3 upgrades, life-support supplies, and trade telemetry in the station window.
2. **Frontier Edge Minerals:** Owned rail platform beside a signed Phase 3 void boundary tile for Edge Conduit construction. Land Area Information shows live extraction metrics (+100% Frontier bonus).
3. **CST Prefab Staging Area:** Pre-leveled 16x10 staging pad in World 2 for stamping canonical CST blocks from the Blueprint Library (`B`).
4. **Custom Blueprint Capture Layout:** Pre-built sample track layout in World 2 for testing `Capture From Map` and custom library persistence.
5. **Oaktree Core Megacity & Governance:** Anchor site for inspecting 3-tier commodity demands, growth states, Supply Chain Matrix & Trade Ledger, and Federation Authentication & Charters.

The save contains an active human company with initial funds, ready for immediate construction.

## Launch

```bash
./build/openttd -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
```

Follow the step-by-step instructions in [SPRINT28-UAT.md](SPRINT28-UAT.md) or open the in-game Story Book (`Manage Company` > `Story Book`).

## Regenerate

To deterministically regenerate the save from source:

```bash
./build/openttd -v null:ticks=200 -s null -m null -b null \
  -c demo/uat_demo.cfg -x -G 9032026 -g
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
./build/openttd -v null:ticks=1000 -s null -m null -b null \
  -c demo/uat_demo.cfg -x \
  -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
```

Validate the generated save with:

```bash
./build/openttd -q demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
```
