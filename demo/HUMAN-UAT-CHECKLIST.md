# OpenSpaceTTD Human Graphical UAT Checklist

Use this checklist alongside [HUMAN-GRAPHICAL-UAT-GUIDE.md](HUMAN-GRAPHICAL-UAT-GUIDE.md) to record human visual playthrough observations. Do not assume automated test passes imply graphical acceptance.

---

## Session Metadata

| Field | Value |
|---|---|
| **Tester Name / Handle** | |
| **Testing Date** | |
| **Build Commit SHA** | `bc18c2c8fc` (PR #44 / #45) |
| **Binary Hash (SHA256)** | `sha256sum build/openttd` |
| **Savegame Tested** | [ ] `demo/OpenSpaceTTD-Integrated-Economy-UAT.sav`<br>[ ] `demo/OpenSpaceTTD-Integrated-CST-Sector.sav`<br>[ ] `demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav` |
| **Display Resolution** | e.g. 1920×1080 / 2560×1440 / 3840×2160 |
| **GUI Scale Tested** | [ ] 100% (1.0x) · [ ] 125% (1.25x) · [ ] 150% (1.5x) · [ ] 200% (2.0x) |
| **Interface Language** | [ ] English (UK) · [ ] English (US) · [ ] Other: ______ |
| **OS / Display Server** | Linux (X11 / Wayland) / Windows / macOS |

---

## Section A: UI Scaling & Text Wrapping Audit

Check each custom panel at your chosen GUI scale. Look specifically for string clipping, word wrapping, button label overflows, and layout scaling.

| Window / Panel | UI Elements Inspected | Status (`Pass` / `Fail` / `Warn`) | Observations & Visual Quirks |
|---|---|:---:|---|
| **Fund Industry Window** | • Survey button (`Survey Sites`) fits bevel<br>• `Economic role: Frontier/Industrial/Core`<br>• Multi-line recipe input/output wrapping<br>• `Required Site: Survey required` notice | | |
| **Resource Surveys Browser** | • Window caption and 16×16 survey tool<br>• Discovered sites list table & scrollbar<br>• Action buttons: `Survey Area`, `Go To`, `Build`<br>• Resizing behavior (matrix stretches cleanly) | | |
| **Industrial Facilities Dashboard (`Ctrl+I`)** | • 5 tabs (`All`, `Pipe A`, `Pipe B`, `Pipe C`, `Pipe D`)<br>• Monthly batch capacity display (`100/mo`)<br>• Input buffer meters (no overflow text)<br>• Output buffer meters (no overflow text)<br>• `Upgrade All` button alignment | | |
| **Station Window / Logistics Hub** | • Multi-line warehouse storage notice<br>• Storage earns £0 final delivery text<br>• Input buffer and production status lines<br>• No text clipped by station detail panels | | |
| **Corporate HQ Dashboard** | • 6 tabs (`Overview`, `Stockpiles`, `Hubs`, `Fab`, `Tech`, `Treaties`)<br>• Campus tier header panel<br>• Planetary stockpile cargo rows<br>• Tech Tree R&D project details & budget buttons<br>• Acceleration toggle button | | |
| **Universe Directory & Star Map** | • 7-world star nodes and link lines<br>• World badges (Core Gold, Industrial Cyan, Frontier Green)<br>• CST public backbone rate display<br>• Gate Access toggle (`Public` / `Private`)<br>• Gate Commissioning project panel & progress bar | | |
| **Megacity Overview Window** | • Header: Population, Growth State, Countdown<br>• Tier 1 (Food) demand & reserve bars<br>• Tier 2 (Steel, Ballast) demand & reserve bars<br>• Tier 3 (Chips, Crystals) demand & reserve bars<br>• Starvation warning text wrapping | | |
| **Blueprint Library (`B`)** | • Purchase mode button (`Cash` / `Stockpile`)<br>• Prefab layout listing & scrollbar<br>• Action buttons (`Capture`, `Place`, `Rotate`, `Flip`)<br>• Multi-line blueprint description panel | | |

---

## Section B: Gameplay & Mechanics Verification

Execute the functional gameplay verification tracks from the [How-To Guide](HUMAN-GRAPHICAL-UAT-GUIDE.md).

| Track | Objective & Verification Criteria | Status (`Pass` / `Fail`) | Notes & Findings |
|---|---|:---:|---|
| **Track 1: Star Map & World Roles** | 1. All 7 worlds visible (Mito Core, Merredin/Tandil Industrial, others Frontier).<br>2. CST backbone link marked Public with toll rates.<br>3. Gate Access switches between Public and Private.<br>4. Promoting a colony preserves its Frontier economic role. | | |
| **Track 2: Paid Resource Surveys** | 1. Fund Industry displays role, tech gate, and survey requirement.<br>2. Survey Area tool highlights 16×16 tiles; cost is deducted.<br>3. Hidden resource sites discovered and added to company list.<br>4. Unsurveyed mine placement is strictly rejected.<br>5. Building on a discovered site succeeds. | | |
| **Track 3: Physical Factory Operations** | 1. Facilities dashboard (`Ctrl+I`) shows active factories.<br>2. Feeder starvation: cutting Iron Ore halts Steel Mill batches (0 produced).<br>3. Backpressure: full output buffer halts production until collected.<br>4. Unrelated warehouses receive zero automatic cargo transfers. | | |
| **Track 4: Logistics Warehouses & Priority** | 1. Warehouse station window displays explicit storage priority notice.<br>2. Priority catchment: freight routes into warehouse before nearby consumers.<br>3. Storage deposits earn £0 commercial revenue.<br>4. Reserve floor slider in HQ restricts outbound loading below floor. | | |
| **Track 5: Tech Tree & Research Escrow** | 1. Tier III project pauses progression waiting for material kit.<br>2. Delivering 40 Steel + 20 Chips atomically reserves the kit in escrow.<br>3. Switching projects refunds reserved escrow back to stockpile.<br>4. Acceleration toggle is OFF by default and cannot spend reserved kit goods. | | |
| **Track 6: Material-Gated Construction** | 1. Cash mode electric rail/maglev requires physical stock; shows missing bills if absent.<br>2. Construction succeeds when materials present and deducts stock.<br>3. Blueprint Library purchase mode toggles between Cash and Stockpile mode.<br>4. CST prefab stamps cleanly onto terrain and deducts materials. | | |
| **Track 7: Megacity Basket & Starvation** | 1. Megacity Overview shows balanced Tier 1, 2, and 3 demand bars.<br>2. Cutting off food for >3 months halts growth and freezes timer.<br>3. Passenger and mail production drops by 50% during starvation.<br>4. Restoring food replenishes reserves, clears warning, and resumes growth. | | |
| **Track 8: Gate Commissioning & Orders** | 1. Consists traverse portal wormholes smoothly without wagon snapping.<br>2. Commissioning gate receives delivered Steel + Machine Modules.<br>3. Once 100% delivered, Activate button opens link to destination.<br>4. Train timetable accepts `Remote stop` and traverses inter-world route. | | |
| **Track 9: Organic Layouts & Infrastructure** | 1. Contoured organic terrain with natural biome hills ($|\Delta h| \le 1$).<br>2. Multi-block street grids with town buildings facing avenues.<br>3. 2-track central terminals with scissors crossovers & PBS signals.<br>4. Holding sidings and engine depots properly positioned.<br>5. Pre-seeded Iron Ore freight consist operates round-trip via portal. | | |

---

## Section C: Defect & Usability Log

Record any visual anomalies, text clipping, layout breakage, or gameplay balance issues encountered:

| ID | Window / System | Severity (`Blocker` / `Major` / `Minor` / `Tweak`) | Description & Reproduction Steps |
|---|---|:---:|---|
| **DEF-01** | | | |
| **DEF-02** | | | |
| **DEF-03** | | | |

---

## Section D: Final Acceptance Recommendation

* [ ] **Accepted:** Graphics, UI text wrapping, scaling, and gameplay loops meet quality standards for human release.
* [ ] **Accepted with Minor Tweaks:** Core gameplay passes; cosmetic text or button adjustments noted above can be resolved in follow-up sprint.
* [ ] **Rejected:** Visual blockers, layout collapse at tested UI scales, or critical gameplay regressions identified.

**Sign-off:** `___________________________` &nbsp;&nbsp;&nbsp;&nbsp; **Date:** `______________`
