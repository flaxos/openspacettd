# OpenSpaceTTD player UAT — recovery checklist, Sprints 1–42

Reviewed against source `abbcd7e077` on 2026-09-15. This remains the authoritative
player checklist; [results](UAT-RESULTS.md) record observations, and the
[master plan](../docs/RECOVERY_PLAN_2026-09-15.md) orders repairs. Start with UAT-00.
Do not ask a player to run the broad suite while its crash smoke still fails.

## Fixtures and distinct suites

| Fixture / suite | Exact input / supported preparation | Acceptance boundary |
|---|---|---|
| **M1: migrated regression** | Copy `OpenSpaceTTD-All-Features-UAT-v1.1.sav`, SHA256 `20f9057fccf0262f7b17a5a1aef4bb6999354ac0ee31f3f9a940b64f4d884f0b`, save367, no active NewGRFs, GS9, first human company0. Use current recorded build. | Existing saved content/ownership/regression; never add replacement GRFs to it. |
| **BC: recorded Blueprint failure** | Copy `/home/flax/.local/share/openttd/crash20260915082450.sav`, SHA256 `503cc9797197b5d2699c52a6cbc4808be6594b4d7a6f1b6a95b6ada113e88b03`, save367/no NewGRFs/GS9. Original build modified a6b79add, not current. | Developer-controlled placement replay, not a fresh-game content fixture. |
| **S1: single-map route** | M1 working copy plus normal player-built depot/stations/train using setup R below. Name stations `UAT Source` and `UAT Destination` and record coordinates in result sheet. | Moving local portals, real cargo and persistence. The named stations do not preexist merely because this guide names them. |
| **P0/P1: processing fixtures** | P0 needs a Developed world with **no company hub anywhere on it**. P1 uses M1's verified World2 hub. Existing M1 cannot be assumed P0. WP-Q must deliver/hash an unhubbed Developed fixture if none available. | P0 currently Blocked until supplied; P1 available only after station/HQ preflight. Distinct destination rules. |
| **C1: fresh active-content** | From title→NewGRF Settings, add exact approved industry/rail versions then Apply Changes; create **New Game**. Record build/save version, IDs/hashes, seed/map/date/settings and loaded cargo/vehicle lists; save a new named fixture. Never change M1. | Currently Blocked: pack runtime/label mapping and fully specified playable fixture need WP-11. NewGRF activation alone is not acceptance. |
| **F1: real federation** | Operator supplies two distinct game saves/configs + authority state, exact binary/content hashes, global/company/gate/station IDs, ports and known train/order route. | Currently Blocked for natural route/recovery; old API kit and manual dispatch are separate evidence. See UAT-16. |
| **V1: UI/visual** | M1 for current windows/procedural visuals; C1 plus provenance-cleared art for bespoke acceptance. Record graphics set/version, zoom, resolution, UI/font scale and language. | GUI evidence required; bespoke asset acceptance currently Blocked. |

### Common preflight and reset (applies to every test below)

1. Record executable version/hash, fixture filename/SHA256/save version, active
   NewGRFs and base graphics, company name/ID, language, simulation date and pause
   state. A developer can obtain version with `./build/openttd -h` and save metadata
   with `./build/openttd -q <copy.sav>`; these do not perform human UAT.
2. Launch the working copy. Use the first human company. Save through the disk
   menu→Save Game using a new case-specific filename; never overwrite M1/BC.
3. Pause using the toolbar pause button. Open Map→**Corporate Headquarters &
   Stockpiles**. Record Overview, **Planetary Stockpiles**, **Logistics Hubs**,
   **In-Kind Fabrication**, **Commonwealth Tech Tree**. Where HQ exists, click
   **Toggle Mode** until standard cash mode for construction cases. Record actual
   local cargo counts and current research; fixture seeding is not proof of them.
4. Cash must exceed the quoted case costs plus operating reserve. Record actual
   amount before/after. If insufficient, use ordinary company finance borrowing
   only if available, or mark Blocked; never use an undocumented money cheat.
5. Find locations using **Map→Sign list** and click the relevant sign row, or
   Ctrl+Alt+1…6 to jump worlds; World1 means internal0. Use toolbar **Land Area
   Information** (question-mark menu) and click tiles to verify real owner/type,
   world and coordinates. Pins are navigation only; inspect the actual fixture.
6. Evidence for every test: before/after screenshots, literal error, dates/event
   counts, involved objects/coordinates and quantities; for crash, retain local
   log/save/screenshot. Record **Pass / Fail / Blocked / Not run** per subcase.
7. Reset every independent subcase by closing its working session and reloading
   the untouched fixture copy. Pause/stop test trains before optional local cleanup;
   delete only your temporary test files if no longer needed. Keep failure evidence.
   No tests in a live shared game. Do not overwrite authority/player saves.

Current visible labels above were checked in source, not graphically demonstrated.
If a label/control differs on the recorded build, capture it and mark the dependent
step Blocked instead of substituting a guessed button. Historical Story Book
chapters/signs mention `Capture From Map`, `B` and proposed panels: current library
button is **Capture**; use the actual toolbar and this checklist.

### Setup R — a normal player-built two-station train route

Use only after UAT-00 passes. In M1, Map→Sign list→**Gateway Alpha - World 1 head**
(the row also includes its axis); repeat **World 2 head**. Inspect both actual owned
terminals. If missing/foreign, stop with UAT-01 failure; do not bypass ownership.

1. Click the main rail-construction toolbar. Select the available conventional
   railway type; keep that type for depot, stations, approaches and locomotive.
2. At each outer terminal approach, use Autorail to connect a short straight line
   to clear land. Click Build railway station, choose matching orientation,
   one platform and length5, then click a site connected to the line. Open the
   station and Rename it to `UAT Source` / `UAT Destination`. Confirm owner/world
   with Land Information. Record coordinates; terrain/occupation rejection is a
   fixture issue, not permission to clear unrelated structures.
3. Click Build train depot, choose entrance facing the approach, click beside it,
   connect with Autorail. Click depot→New Vehicles; select an available conventional
   locomotive→Build Vehicle, then a compatible freight wagon→Build Vehicle. Keep
   total consist length <=5. Verify the wagon belongs to that engine in the depot.
   Record exact engine/wagon names and cargo; no invented CST vehicle required.
4. Open train→Orders→Go To; click `UAT Source`; click Go To again and click
   `UAT Destination`. Return to normal cursor. For the transit-only case leave
   ordinary loading; for cargo cases set source full load only when it supplies
   this cargo, destination ordinary unload/No loading as specified below.
5. Click the train's stopped/start status bar, unpause and follow its viewport.
   Observe a full round trip, including every wagon. A missing route/blocked
   signal requires diagnosis. Deadline: two economy months; record elapsed time.

Cargo extension: Map→Industries (industry directory) select a live source matching
the wagon; centre its view. Build the source station with catchment showing that
producer. At destination choose matching accepting industry or the UAT-14 facility;
station window must show acceptance. Connect these stations via the same native
rail/portal steps. Record actual production/acceptance; a map label is insufficient.

## Short smoke gate

### UAT-00 — main windows and first Blueprint placement (10-minute cap)

**Fixture/resources:** M1 then BC for regression; company0, World2 developed,
cash mode, flat owned-world pad at least8x2 plus clearance. No materials/research
required for conventional track. **Status:** Not run current; historically failed
placement confirmed. Run deliberately on copies after WP-01; stop broad UAT on fail.

1. Open/close each Map menu entry: Universe Directory, Freight Corridor Monitor,
   Supply Chain & Trade Ledger, Federation Authentication & Charters, Corporate
   Headquarters & Stockpiles. Click each HQ tab. Open a real station and company
   details. Capture any rendering/assertion failure.
2. Open rail toolbar; hover the Blueprint icon until **Open Rail Blueprint Library**
   tooltip, then click. Record opening separately from selection.
3. Click **CST Mainline Double Straight (8x2)**. Record selection and preview.
   Click **Rotate CW** once, then three more times; click **Flip** twice. Confirm
   preview returns to initial arrangement. These are separate observed substeps.
4. Click **Place**, click the empty pad, press Escape. Check all track pieces,
   signals and cash. On BC also repeat at the recorded scene; exact original tile
   is unknown, so record the chosen tile and full footprint.
5. On a reset copy place over a deliberately incompatible/obstructed footprint.
   Expected clean error, no partial layout, no material/money change. Do not use
   another player's real infrastructure as a disposable fixture.
6. Save/reload test copy; reopen library/station/company windows. Expected no crash
   and retained placed state. Capture video/screenshots and counters. **Reset:**
   common reset; preserved original crash is not replaced by this result.

## Player cases

Each case inherits common evidence/status/reset and setup rules. Substeps that
require undelivered fixtures are explicitly Blocked, not implied passes.

### UAT-01 — ownership, approaches and local full-consist transit

**Fixture:** S1 prepared from M1; company0, Worlds1/2, cash for setup R, fabrication
off, conventional rail; no research/material gate. **Status:** Not run current;
prior ownership complaint retained pending retest.

1. Locate Gateway Alpha through Sign list. Land Information→click each head,
   holding lane, switch and outer connection; capture company ownership. Repeat
   ownership inspection at Beta–Epsilon signs, not travel acceptance yet.
2. On an unoccupied outer rail, rail toolbar→Remove tool→click one piece; turn
   Remove off and rebuild same rail. Use signal tool to remove/rebuild an unoccupied
   path signal. Record cost and ensure neighboring tracks remain.
3. Execute setup R; watch all wagons cross and return, orders intact. Save/reload
   mid-route using a new file, finish return and reinspect ownership.
**Negative:** occupied/reserved rail must reject removal safely. **Expected:**
no foreign-owner error on supplied owned rails, usable route and persistence.
**Evidence:** owner captures, orders, both-end video, balances, save. **Reset:** common.

### UAT-02 — worlds, phases, biomes and navigation

**Fixture:** M1/V1, company0; no cash/material/research requirement. **Status:** Not run.

1. Press Ctrl+Alt+1, capture world label; repeat2–6 at identical zoom.
2. Open Map→Universe Directory, select each row→**Jump Viewport**; compare with
   hotkey destination and phase/biome shown. Inspect a real boundary with Land Info.
**Negative:** try conventional rail on void in a working copy; expect rejection,
no tile/cash mutation. **Expected:** correct six destinations and visible phase/
terrain distinctions; original art not implied. **Evidence/reset:** common + six captures.

### UAT-03 — build/link and protect portal terminals

**Fixture:** M1 working copy, company0, clear level pads in Worlds1/2 with room for
18-tile approaches and two holding lanes, conventional rail/cash mode, sufficient
quoted funds; no research/materials. **Status:** Not run.

1. Rail toolbar→Portal icon (tooltip describes automatic two-lane terminal).
   **Portal Gate Builder**→**Build gate**→NE/SE/SW/NW orientation; ensure highlighted
   approach is clear; click gate site. Record built full terminal/unlinked state.
2. Jump other world; choose suitable orientation and place second gate.
3. Click **Link gates**; click first existing unlinked head, jump and click second.
   Use setup R with those approaches, start train, observe both directions.
4. While a consist is in transit, bulldoze gate on a throwaway copy; expect clean
   occupied rejection. Reset; after route is clear, bulldoze one head and inspect
   remaining unlinked head/approach rails, then save/reload.
**Negative:** choose two same-world heads or obstruct late terminal footprint;
expect error and no partial linking/build. **Evidence:** exact orientations/tiles,
reservation timing, errors, full footprint and costs. **Reset:** common.

### UAT-04 — Blueprint sub-actions (never aggregate partial results)

**Fixture:** M1/BC after WP-01; company0, World2 pad via Sign list row beginning
**UAT CST Prefab**; inspect pad exists. Cash mode first, then separately fabrication
with recorded adequate inventory/research. **Status:** recorded placement Fail;
user reports the earlier placement smoke passing. WP-06/07 storage, capture and
route repairs are now implemented locally. All04 subcases still need visual
acceptance against this newer build; automated train tests are recorded separately.

| Subcase | Literal action / expected visible result | Negative and evidence |
|---|---|---|
|04a Open/select/preview | Rail toolbar→Blueprint Library; click each built-in row, scroll list; distinct preview/name/dimensions without crash | Empty selection must disable irrelevant actions; capture selected row+preview |
|04b Rotate/flip | Select block→Rotate CW four separate clicks→Flip twice | Return to initial geometry; capture each orientation; not a single Rotate(4) data check |
|04c Place | Select→Place→click distinct clear pad→Escape for each of eight blocks | Full footprint/signals, exact quoted/actual cash/materials; obstructed final tile gives no partial build; all screenshots |
|04d Route | Connect depot/test train to each advertised entrance/exit using setup R; issue destination order/start | Every advertised movement reaches destination, no deadlock in two economy months; route/order video; use revision 2 built-ins, including all six Wye movements and both portal-corridor turnbacks |
|04e Capture/name | Build small owned three-piece rail sample; Capture→mouse-down first corner→drag beyond sample→release; enter unique name→confirm | Expected name dialog/new row/persisted preview. Switch Place→Capture and Capture→Place; repeated clicks must keep the chosen tool working. Empty/foreign area rejects clearly; Escape during selection and Cancel at naming save nothing |
|04f Export/import | Use rebuilt WP-06. Select a built-in with no existing player copy; optionally Rotate/Flip→Export→accept or enter a new JSON file path→OK. Verify the actual file. Close/reopen→Import→enter that file path→OK; compare layout/name and editable player row | Existing destination and malformed input must report failure without altering files/list. Duplicate player names reject: for an own-template roundtrip, rename the original row before importing its earlier export. Cancel must create nothing. Record paths and layout/hash evidence |
|04g Rename/delete | Select own row→Rename→unique name→confirm; close/reopen; Delete only disposable own row | Built-in Delete must reject; duplicate-name rename must fail without changes. Successful rename retains the existing backing filename and survives reopen; Delete removes only that row's file. Record filesystem/error evidence |

**WP-01 retest boundary:** the rebuilt command supports every built-in footprint
on clear land. Ordinary trees may be cleared under track/depot cells. Station
cells need clear land or complete matching owned station overlap; trees under
stations, partial/custom/irregular station overbuilding and multiple new station
blocks reject. Railtype/depot conversion and clearing roads/buildings/objects
also reject. For the negative case, obstruct the final occupied tile with foreign
infrastructure and confirm that the entire stamp leaves map, money and materials
unchanged. Repeat an exact successful stamp: no extra charge or material use.

**Reset:** common between stamps; preserve user library. Never delete existing user
blueprints. Imported invalid-enum/pool/file-I/O cases are developer regressions,
not instructions for a player to crash or damage their library.

### UAT-05 — spaceport output and conduit station delivery

**Fixture/resources:** M1, company0; World1 owned **Phase 1 Spaceport Candidate**
airport and World3 **Frontier Edge Minerals** actual rail station, located from
Sign list. Verify ownership/existence/catchment; if absent, build owned airport/
rail station with native toolbar or mark Blocked. Cash mode; no custom research.
**Status:** Not run; OST-EC-001 counter defect known from code.

1. Click actual airport station→**Designate Spaceport**. Record tier, supplies,
   **Next monthly off-world trade** and cash. Deliver actual requested supply cargo
   with normal delivery, then click **Upgrade Spaceport to Tier…**. Capture success
   or exact requirement error; run through next calendar/economy monthly update.
2. At World3 boundary sign inspect actual adjacent void and rail approach. Click
   rail toolbar Edge Conduit→click boundary tile; click Land Information then
   conduit to record selected mineral type and extraction counter.
3. Open nearby mineral rail station: catchment must cover conduit. First run one
   monthly event **without loading service** and record waiting/counter. Zero waiting
   may be valid even when nominal production counter grows (the misleading claim
   is the defect).
4. Reset. Build depot/connect station; buy a locomotive and wagon/refit for the
   conduit's actual mineral. Train Orders→Go To→mineral station; keep loading enabled
   (not No loading), start it and let it try loading. Then supply destination order
   to an actual accepting station using setup R. Record station waiting, onboard,
   downstream accepted and counter before/after next monthly event.
5. Compare actual delivered totals, not just production. Default service filter,
   ratings, competition and packet allocation affect quantities; do not expect
   exactly100 waiting from100 nominal. Stop service for next month as control.
**Negative:** click inland non-void-adjacent tile with conduit tool, expect clean
rejection; selecting existing conduit removes it, so reset first. **Evidence:**
catchment/owner, train cargo/orders/service, monthly date, counter/waiting/onboard/
accepted table, upgrade error. **Reset:** common; remote piping belongs UAT-16.

### UAT-06 — Megacity supply, growth and corridor readings

**Fixture:** M1/S1, company0, Core registered Megacity and funded matching supply
train; no assumed special research. **Status:** view Not run; sustained-supply
subcase Blocked unless real matching delivery route is prepared.

1. Map→Universe Directory→Core row→**View Megacity**→**Locate Town**. Record displayed
   actual cargo quotas/satisfaction and population, not only narrative role names.
2. Run ordinary deliveries for displayed needs; capture accepted quantities and
   next monthly satisfaction/growth. Follow for three supplied monthly updates;
   reset and observe three unsupplied updates as control.
3. Open Map→Freight Corridor Monitor before/after the S1 train run. Record whether
   the selected route is local or external and actual counter/time source.
**Negative:** absent corridor/zero external traffic cannot pass congestion because
local trains move. **Expected:** measured supply explains corresponding growth;
no delivery still credited twice through hub. **Evidence/reset:** common + monthly series.

### UAT-07 — ledger and governance windows

**Fixture:** M1, company0, no external connection; no cash/material requirement for
view. Local test identity uses synthetic text only. **Status:** Not run; external
transaction effects Blocked until F1.

1. Map→Supply Chain & Trade Ledger→**Supply Chain Matrix**, then **Trade Balances &
   Conservation**, click **Refresh**; record actual rows and data source.
2. Map→Federation Authentication & Charters→**Log In / Register**; enter
   `AuditPlayer`→confirm. Click **Charter Corporation**→enter `AuditRail`→confirm.
   Select its charter row→**Register World Presence**; record which actual world
   was registered (current code chooses the first region, not the viewed world).
   Click **Authorize Delegate**, cancel the dialog and verify no delegate added.
   Capture local session/owner/presence and errors; the Online text is not proof
   of a remote authenticated service.
3. Compare ledger after a real accepted S1 delivery where that ledger is applicable.
**Negative:** before login, Charter Corporation must reject; cancelling a query
must preserve state. Duplicate behavior must match the actual registry policy.
**Expected:** actions have visible effects/errors; static CONSERVED label is no
physical conservation proof. **Evidence/reset:** common; no real credentials in logs.

### UAT-08 — colonisation, settlement, promotion and restrictions

**Fixture:** M1, company0; Directory-selected actual uncolonised Phase4 World4–6,
quoted colony cash, no special material/research gate assumed. **Status:** Not run;
WP-05/08 command authority and native town initialization are implemented locally.
Use the current rebuilt executable and a Phase4 world without an existing empty town.
Older incomplete outposts are preserved on load; automatic repair is unsupported.

1. Map→Universe Directory→Phase4 row→Jump Viewport. Attempt conventional depot on
   untouched world; record restriction before founding.
2. Click **Found Colony**, capture command result/news, actual town buildings,
   population and phase separately from directory badge. Jump to town and inspect
   it; a new sign alone does not pass. Save/reload and inspect again.
3. Deliver cargo normally; record development score before/after monthly events.
   Click **Promote World** below threshold on a separate copy, then after legitimate
   score reaches displayed threshold. Expected denial without badge/state change,
   or exactly one phase advancement and newly allowed construction.
4. Try unavailable advanced rail/depot and volcanic bio-farm through ordinary
   construction menus; capture actual rule/error. If relevant content/tool is
   unavailable, mark that subcase Blocked, not passed by a news message.
**Negative:** insufficient-funds founding must leave town/phase/directory unchanged;
operator supplies low-funds copy if normal finance cannot produce it. **Evidence:**
real town/phase/score/cash before+after+reload. **Reset:** common, no console promotion.

### UAT-09 — persistence and crash-window regression

**Fixture:** S1 after one valid progression action from repaired UAT-08, company0;
record all cash/cargo/research states. **Status:** load smoke passed automatically;
human Not run, progression extension Blocked until08 passes.

1. Pause with train partway along route, record order/consist/cargo/ownership.
   Disk menu→Save Game→new name; close game; relaunch/load that file.
2. Open Company details, Story Book, station, train Orders, Directory, HQ and
   Blueprint Library. Resume and complete delivery.
**Negative:** compare copied older M1 without new progression; it must still load,
not inherit prior game's manager state. **Expected:** no invalid-string/window
crash, preserved links/owner/orders/cargo/progress, no duplicate tutorial pages.
**Evidence/reset:** common plus both saves and continued-delivery capture.

### UAT-10 — HQ, stockpile and hub player workflow

**Fixture:** M1 existing HQ/hub, company0, Core HQ/World2 hub; locate via HQ
**Locate Campus**, hub tab and station list **Merredin UAT Logistics Hub** if present.
Check actual owner/world/platform; no assumption sign is the attachment. Cash,
material counts and reserve floors recorded from UI. **Status:** WP-09 establishment
and reserve controls are implemented locally; graphical acceptance is Not run.
For fresh establishment use a copy with no Corporate HQ, at least 5,000,000 Cr cash,
a Core World site and owned live rail stations on Developed and Frontier Worlds.

1. Map→Corporate Headquarters & Stockpiles→Overview→Locate Campus; record tier.
   Click Planetary Stockpiles and Logistics Hubs; record real cargo counts/binding.
2. With the rebuilt WP-05 executable on a working copy, click **Upgrade Tier**.
   Require an existing owned HQ below tier4; the existing tier step has no fee.
   Record one tier change, unchanged cash and persistence after save/reload.
   If two clients are available, both must show the same tier. A foreign HQ view
   must not allow upgrade, research-budget or fabrication changes. Rapid clicks
   while the same target tier is pending must not skip another tier.
3. With the WP-02 rebuilt executable, unload a known freight amount at the verified
   owned hub using an ordinary delivery/unload order and **No loading** for this
   check. Use cargo without a matching station-processing recipe. Expect that
   amount added once to stockpile, no native industry/town consumption and no sale
   income or delivery/development credit. Record waiting/onboard, input, stockpile
   and cash; account for ordinary running costs separately. Save/reload a new copy
   and check the balance persists. Station processing retains priority for its
   inputs; Transfer still leaves waiting cargo and No unloading keeps cargo onboard.
   With the WP-03 rebuilt executable, separately enable ordinary loading and
   **No unloading** at the hub. Record the existing reserve floor and stock,
   waiting cargo and train capacity before pickup. Only surplus may leave stock;
   stock+waiting+onboard must stay constant. Repeat at the floor, then save/reload
   a new copy. Pool exhaustion is covered by developer fault injection; no manual
   attempt to fill the cargo pool is needed.
4. On separate disposable copies using the WP-04 build, remove one platform tile;
   the existing hub should remain if a same-world owned platform stays within four
   Manhattan tiles of its anchor. Remove the final platform: the hub entry and hub
   pickup disappear immediately, while planetary stock remains unchanged. Reload
   the saved copy and verify that the removed hub does not return. Invalid or
   duplicate old bindings may be removed on first load; record the before/after
   association and unchanged stockpile quantities.
5. On the fresh-establishment copy, open Corporate Headquarters and click
   **Build Hub**, then **Establish HQ** twice, then a Core World tile. The last
   selected tool must remain active. Expect one HQ and a 2,500,000 Cr charge.
   The Core site counts as presence; local owned rail stations on Developed and
   Frontier Worlds supply the other two phases. Registered charter presences also
   count. Try an invalid/poor-company copy separately and expect no HQ or charge.
6. Click **Build Hub**, then one of your rail station platform tiles. Expect a
   single station attachment and a 75,000 Cr charge. Empty ground, foreign stations
   and duplicate attachments must reject without charging.
7. Click **Select Hub** until the desired hub appears, then **Cargo** until the
   desired cargo appears. Click **Set Reserve** (shown as **Reserve: N** after
   selection), enter 40 and confirm. Cancel and an out-of-range value must preserve
   the previous floor. With 60 units of that freight stored, ordinary loading may
   withdraw 20 and must leave 40; use normal unload/load orders as in step 3.
8. Save to a new file, reload, and verify HQ location, hub/station binding, reserve,
   stock and onboard cargo. Recheck Fabrication and Tech Tree controls in 11/12.
   Native acquisition/bankruptcy is covered separately; hub ownership transfer does
   not imply migration of every company inventory/HQ/research ledger.
**Negative:** invalid/foreign station binding rejected with no new hub or charge. **Evidence:** actual association, physical quantity balance, limit behavior,
save/reload. **Reset:** common; no setup_uat_fixtures reseeding to fake deliveries.

### UAT-11 — in-kind cost and material accounting

**Fixture:** M1 company0, World2 clear five-tile conventional straight sample,
HQ seeded inventory verified; record actual cargo counts (shared alias roles are
one pool), enough cash. No research needed for baseline80%. **Status:** Not run.

1. HQ→In-Kind Fabrication→Toggle Mode to cash. Build five identical rail pieces
   with Autorail; record cash/material delta. Reset.
2. Same location/type→Toggle Mode to in-kind; repeat five pieces; inspect cash and
   actual local stockpile after. Test depot, signal and locomotive separately,
   each with their displayed requirements and independent reset.
3. Repeat with an operator-supplied low-material fixture after WP-Q; require clean
   rejection/fallback exactly as the decided rule states, no hidden deduction.
4. After completing Materials3 in UAT12, compare90% discount separately; do not
   infer research from a status label. Blueprint aggregate construction is04.
**Negative:** insufficient combined requirements where roles share one cargo.
**Expected:** each item consumes integer BOM once; discounts agree with real cost.
**Evidence/reset:** common + item count, exact cargo IDs/counts, cash ledger.

### UAT-12 — research funding, prerequisites and actual effects

**Fixture:** M1 eligible Core HQ/company0, funded cash >=selected monthly budget;
record existing tech states and feedstock. Cash-only research is supported; missing
crystals alone does not block baseline. **Status:** Not run.

1. HQ→**Commonwealth Tech Tree**. Click main technology panel to cycle selected
   projects; record node/requirements. Click **Start Research** on an available node.
2. Click **Budget** to cycle0→25,000→50,000→100,000→250,000→0; choose25,000 for
   controlled run. Record RP/cash/actual HQ feedstock just before month change.
3. Unpause across monthly processing; compare funded RP/cash/feedstock. Full budget
   must be affordable. Set budget0 for control (feedstock can still contribute).
4. On reset select locked prerequisite project→Start Research; expect error/no
   change. Continue valid project to completion, save/reload and test its actual
   defined behavior (e.g. Materials3 discount/yield). Traction purchase requires C1.
**Negative:** insufficient cash for complete budget leaves cash funding unspent;
feedstock effects recorded separately. **Expected:** one project, honest prerequisites,
correct monthly accounting, persistent unlock with verified effect. **Evidence/reset:**
common + monthly ledger and actual purchase/build effect; node completion alone insufficient.

### UAT-13 — fresh Commonwealth content and availability

**Fixture:** C1 only, company0; required date/phase/research/materials specified in
its approved manifest. **Status:** Blocked: fresh validated fixture/runtime labels
and pack semantics need WP-11. M1 cannot pass this case.

1. At title→NewGRF Settings activate exact two pack hashes for new games, then
   New Game. Inspect game NewGRF Settings, Cargo Payment Rates and station/industry
   cargo lists; record all **13 conceptual cargos** and actual loaded labels/IDs.
2. Build compatible depot on allowed world→New Vehicles; inspect actual CST
   engines/wagons and refit lists. Buy one available vehicle, run its cargo route.
3. In approved before/after-research fixture repeat purchase and wrong-phase
   purchase. Record both calendar and research restrictions, not assumed replacement.
**Negative:** incompatible/missing pack fixture must reject clearly or use explicitly
supported fallback; never inject replacement GRFs into old save. **Expected:**
real cargos, industries/refits/vehicle behavior, stable reload. **Evidence/reset:**
manifest/screenshots/cargo route; discard only C1 working copy and restore new-game presets.

### UAT-14 — input → processing → onward useful delivery

**Fixture:** P0 unhubbed Developed world for station output, P1 for hub output;
company0, cash >=100,000 Cr+route costs, no Materials3 for base arithmetic;
60 units actual Iron Ore delivered by train. **Status:** P0 Blocked until fixture;
P1 Not run after hub repairs. Distinct content branches depend on C1.

1. Build owned conventional rail station using setup R. Click station→**Build
   production facility (100,000 Cr)**→**Blast Furnace Structural Steel**.
   Record inputs/output names, buffer and output destination; wrong-phase recipes disabled.
2. Input train Orders→ordinary delivery and **No loading** at refinery (not
   Transfer). Deliver60 ore from actual source; stop further input. Record input
   buffer and cargo received; do not count arrival before unloading as delivery.
3. At next monthly update expect30 batches,30 Steel and no remaining ore. P0:
   steel waiting at station. P1: steel in world stockpile because a company hub
   exists anywhere in that world; do not expect waiting at refinery as well.
4. Run separate compatible output train to accepting final destination/stockpile
   via second local portal. Record accepted quantity and useful consumer/fabrication
   effect, income/development and no duplicate cargo. Stop input; next month no new output.
5. Save residual buffers/output and reload. Click **Remove facility** or demolish
   last platform on separate copy; internal buffers salvaged once, facility gone.
   Partial platform removal preserves facility and valid anchor.
**Negative:** duplicate/foreign station, wrong phase, unavailable recipe or late
obstruction reject atomically. **Evidence:** full input/waiting/onboard/stockpile/
consumer/cash ledger and monthly dates, route video, reload/removal. **Reset:** common.
Additional Pipelines A–D each need equivalent recipe-specific fixtures/ratios;
one steel example does not pass all chains or “12-cargo” economy.

### UAT-15 — UI/visual and bespoke art acceptance

**Fixture:** V1; same graphics version/zoom/UI scale/company; no cash/research for
inspection. **Status:** current procedural visuals Not run; bespoke art Blocked.

1. Repeat02 six-world captures; include active/unlinked gates, rail/prefab routes,
   stations, facilities, settlements, HQ/directory windows and menu at normal zoom.
2. Compare text readability, selection/cursor/error visibility, scaling and
   recognizable biome silhouettes; record exact asset provenance where supplied.
**Negative:** small/large UI scale and overlapping busy background must not hide
controls. **Expected:** readable usable current UI; palette changes do not pass
missing original art. **Evidence/reset:** matched screenshots + issue list; common.

### UAT-16 — real federation: separate physical subcases

**Fixture:** F1 operator manifest; two game processes+authority, compatible exact
content, two identified player companies and station-order destinations, local
copies of every state file. **Status:** Blocked for natural entry/recovery pending
WP-F1/F2 and fixture delivery; protocol/manual-dispatch evidence retained separately.

1. Operator starts bounded cluster per [FEDERATION-UAT](FEDERATION-UAT.md), verifies
   ports from config (current authority38080), provides both company join details,
   real gate/station IDs, initial complete consist/cargo/orders and save checkpoints.
2. Player joins source server, opens the specified train→Orders; verify actual
   route and loading flags, then click Start. Follow its **physical gate entry**.
   Operator does not call federation_dispatch for this subcase.
3. Join destination as mapped company; locate actual restored train via train list;
   compare full consist, per-cargo amounts, global identity, owner, all order flags
   and destination bindings. Observe unloading and **natural ordered return**.
4. Operator repeats from isolated checkpoints with wrong content, blocked arrival,
   lost reply/duplicate delivery and restart/older-save cases; player captures
   expected waiting/error/recovery. Each failure point needs its own case manifest,
   never ad-hoc termination of a live shared server.
**Expected:** exactly one physical custody state, no loss/duplication, matching
orders and safe eventual arrival/return or explicit recoverable block. **Negative:**
missing ledger field is failure evidence, never default true. **Reset:** stop only
fixture processes, preserve logs, restore all related copies together. **Evidence:**
per-subcase source/authority/destination event IDs+saves/video+physical balances.
No existing complete player F1 manifest exists: dependent steps remain Blocked,
not instructions claiming a particular prebuilt remote train/station is available.

## Complete sprint and spike mapping

Every sprint is represented; documentation sprints are reviewed rather than
turned into invented gameplay. Individual player outcomes are recorded above.

| Sprints | Outcome / UAT cases |
|---|---|
| 1, 2 | Worlds and phase restrictions: 02, 08 |
| 3, 4 | Local traversal and generated gates: 01–03 |
| 5 | Cargo attribution, revenue, industry rules: 05–08 |
| 6 | World-aware UI: 02 |
| 7 | Persistence: 09 |
| 8 | Portal construction and demolition: 03 |
| 9, 10 | Planetary operations and crash stabilisation: 05, 09 |
| 11 | Terminal topology/PBS: 01, 03 |
| 12, 13, 14, 15 | Snapshots, admission, identities, transfer: 16 |
| 16 | Accounts, charters, directory, ledger: 07, 16 |
| 17, 18 | Megacities, freight economy and UI: 06 |
| 19, 20 | Cluster recovery and infrastructure routing: 05, 16 |
| 21 | Telemetry/navigation and Commonwealth strings: 02, 06, 13 |
| 22 | Round-trip orders: 09, 16 |
| 23 | Scope/art/UAT audit: this mapping and 15; documentation review |
| 24 | Procedural biomes: 02, 15 |
| 25, 26 | Player blueprints and eight CST blocks: 04 |
| 27 | UI completion: 04–08; record missing controls as gaps |
| 28, 29 | Solo guide and federation acceptance kit: 01–09, 16 |
| 30, 31, 32, 33 | Six biomes, colonisation, development, town/industry lifecycle: 02, 06, 08 |
| 34 | Documentation consolidation: reconcile this guide, roadmap and coverage matrix |
| 35 | Cross-process transport: 16; operator-dispatch evidence is not natural entry |
| 36 | All-feature demo: 01–12; artifact availability is not human acceptance |
| 37, 38 | Content pack and bespoke art: 13, 15; blockers explicit |
| 39, 40 | HQ/logistics and fabrication: 10, 11 |
| 41, 42 | Research and production chains: 12, 14 |

Portal/YAPF and multi-wagon spikes map to 01/03/09; F2 transfer to 16;
F3 persistent universe to 07/16; F4 economy to 06/07. The corporate-tech design
spike maps to 10–14. Their historical automated evidence remains separate from
these unproven human acceptance outcomes.

## Reproduce the artifact

`python3 scripts/refresh_uat_demo.py` migrates the preserved v1.0 using the current
GameScript and acknowledged ownership/save commands. It refuses to overwrite an
existing output; use `--output /tmp/uat-review.sav` for a repeat run. `-X` isolates
script discovery from installed older copies; ensure OpenGFX is available under
`build/baseset/` when using that mode. This is a local artifact operation, not
federation acceptance. Never run fixture setup on a live shared server.

After code changes, inventory CTest once and run the relevant cases, then any
required broader suite. CTest already includes Catch2: do not repeat the same full
suite through openttd_test. Current bounded audit evidence is linked from the
master plan. The existing refresh script's verify mode is an operator artifact
check, not a graphical route pass; run only in an isolated writable directory with
compatible baseset/script paths and no live clients. Record its exact output.
