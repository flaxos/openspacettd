# Feature → implementation → player → verification evidence

## Current user acceptance and direction — 23 September 2026

The user confirms human UAT is OK and accepts the new graphics as good enough
for now. Further graphics improvements are optional later work. This supersedes
older blanket statements that human UAT and graphics acceptance are pending.
This is a user-reported overall acceptance, without new per-subcase captures or
a pinned build/save manifest; historical observations remain unchanged.

The user separately asks whether federation has been tested with two multiplayer
servers and two open clients. That workflow remains unverified; general gameplay
acceptance does not close it. The next priority is the live federation acceptance
pass described in [the current roadmap](PROJECT_STATUS_AND_ROADMAP.md#next-priority-live-federation-with-two-clients).


Audited source `abbcd7e077` on 2026-09-15. The [master recovery plan](RECOVERY_PLAN_2026-09-15.md)
sets dependencies; the [defect register](CRITICAL_BUG_REVIEW_2026-09-15.md) gives
stable IDs. Human procedures/results remain [ALL-FEATURES-UAT](../demo/ALL-FEATURES-UAT.md)
and [UAT-RESULTS](../demo/UAT-RESULTS.md). Implementation and verification are
independent columns. No row is declared complete by its sprint number.

**Verification key:** E67 = the 67 selected existing CTests executed in this audit
(see [exact selection and log](audit/2026-09-15/targeted-ctest.log)); I = source/test
inspection only, not rerun; H = historical evidence only. Full-engine null-video
load smokes exercise startup/restoration, not placement, train routes or GUI.
Production-test qualification: live-main CI fails three production fixtures on
invalid void-edge setup; local preprocessing shows assert is a no-op in that test
translation unit despite WITH_ASSERT. E67 does not prove that invariant; see
OST-TEST-001 and the master plan. Catch2 checks still execute.

WP-02 later repaired the fixture's mandatory void edges and passed 57 distinct
selected CTests, including six new hub delivery cases. Assertion-active CI and
human acceptance remain pending; see [WP-02 evidence](audit/2026-09-15/wp02/README.md).

All human cases are Not run on current build except the identified historical
placement failure and explicit blockers; old ownership report remains open for retest.

| Feature / requirement → design source | Implementation / status | Actual player entry | Automated verification | Human evidence / case | Blocker → next action |
|---|---|---|---|---|---|
| Single tile map, separated worlds, phase/biome model → Game Design §§D/E; Sprint30/32 | `portal/planet_manager`, `world_gen`, `planet_type`; regions, void, six biomes exist; diagram is illustrative | Map world entries, Ctrl+Alt+1–6; Universe Directory | I: world/biome/lifecycle suites | 02 Not run; historic saves exist | No current six-world visual set; inspect generated strip bounds, phase labels and terrain |
| Local arbitrary wormholes and full consist traversal → Portal Wormhole Spike; design §F; Sprint11 | Registry + `tunnelbridge_cmd`, `train_cmd`, YAPF/PBS; implemented | Rail toolbar Gate/Link; train station orders | E67 local portal lifecycle; I consist movement cases | 01/03 Not run; historical portal/conduit UAT is limited | Current human route/ownership retest; preserve closed stale/unlinked heads |
| Safe construction/demolition/save → Sprint10/11; stabilisation record | Validation, reservation clearing, in-transit protections, `planet_sl` | Rail build/remove, save menu | E67 local construction/demolition/persistence | 01/03/09 Not run current | OST-PORT-001 external direct-command crash risk; takeover accounting risk |
| Portable captured rail blueprints → Sprint25; design §T | WP-01/06/07 placement, parser/storage and capture repaired locally | Library Capture→drag→name; Export/Import paths; rename/delete | Native capture/abort/query callbacks, file persistence and admission/failure tests | Earlier placement smoke reported passing; new04 visual retest pending | User acceptance of capture, transformed export/import and cancellation |
| Eight functional CST prefab blocks → Sprint26 | Eight revision2 layouts; older imports retained | Library list, Rotate, Flip, Place; native train orders | Complete placement/transforms plus 312 native train runs (39 movements ×8 transforms), all signals intact and 90-degree turns disabled | 04 visual retest pending | Run every movement with player-built approaches and appropriate train lengths |
| Spaceports and Edge extraction → Sprint9/20 | `spaceport_manager`, `edge_conduit`, station commands; honest allocation implemented (WP-10 / PR #8) | Airport station Spaceport controls; rail toolbar Edge Conduit; LandInfo outcome display | E67 conduit construction/multipliers; 6 WP-10 catchment/service/rating tests (97 assertions) | 05 Automated tests pass; human visual UAT pending | OST-EC-001 resolved; honest allocation and LandInfo verified |
| Useful cross-world payment/progression → design §§C/G/L | `economy::DeliverGoods`, development and Megacity hooks exist | Actual normal-delivery train orders, company/stockpile/phase views | I economy/consist; E67 staged production cargo integration | 01/06/14 no accepted moving vertical slice | WP-02/03 deposit/loading repaired locally; establish one human-accepted conserved route |
| Sustained Megacity supply and visible growth → Sprint17/33 | demand/monthly state/town hooks implemented | Town→Megacity; Map→Megacity Overview | I megacity tests; no current graphical growth run | 06 Not run | Record exact deliveries, monthly quotas, growth and unsupplied control |
| Congestion, directory and ledger → Sprint18/21/27 | `freight_corridor_gui`, `trade_ledger_gui`, service data | Map→Freight Corridor Monitor / Supply Chain & Trade Ledger / Universe Directory | I domain/UI data tests | 06/07 Not run | Local static state is not external traffic evidence; log source and actual events |
| Accounts/charters/presence → Sprint14/16/27 | local registries, commands and authority API present | Map→Federation Authentication & Charters | I identities/charters/protocol | 07 Not run; external acceptance separate | Runtime namespace/owner fallbacks OST-FED-003; isolate external tests |
| Colonise functioning settlements / promote → Sprint30–33 | WP-05/08 authoritative commands, native founding and persistence repaired | Directory row→Found Colony / Promote World / Jump Viewport | Pool/site/caller/funds rejection, houses/population/spatial lookup, directory and save/reload; versioned legacy preservation fixtures | 08 Not run | Visual new-colony acceptance; older incomplete towns are preserved and automatic repair is unsupported |
| World/biome build restrictions and operating freedom → design §H; Sprint32/33 | track/industry/town checks exist; phase construction stricter than early through-running vision; Oceanic rules incomplete | Normal build tools, Directory | E67 portal/blueprint subset; I world restrictions | 08 Not run | OST-DES-001: explicit design decisions before changing rules |
| HQ establishment/tiers/company presence → corporate spike; Sprint39 | WP-05/09 owner-checked commands and placement controls implemented | Corporate Headquarters→Establish HQ→Core site | Fresh HQ GUI/cost/error/presence checks and process reload | 10 Not run visually | Core site plus owned Developed/Frontier rail stations (or charter presence), at least5,000,000 Cr cash; acceptance pending |
| Planetary stockpiles / bidirectional hubs/reserves → corporate spike; Sprint39 | WP-02–04/09 cargo, station authority/lifecycle and reserve controls implemented | Build Hub→owned rail platform; select Hub/Cargo→Set Reserve; normal station orders | Conservation/failure/lifecycle tests plus fresh GUI flow and native depot-to-hub journey:60 deposited,20 loaded,40 reserved, process reload | 10 visual delivery/pickup/editing pending | Confirm player-operated delivery and reserve behavior; native automated travel does not establish human acceptance |
| In-kind cash/BOM dual mode → corporate spike; Sprint40 | FABR, rail/signal/depot/vehicle hooks; 80% base /90% Materials3; integer BOM | HQ→Fabrication→toggle | WP-01 exact aggregate/copy-signal consumption and exhausted-stock overlap covered; broader fabrication tests pass | 11 Not run | OST-BP-001/HUB-001; shared aliases must be aggregated, no material bypass |
| Research progress/unlocks → corporate spike; Sprint41 | TECH/prerequisites/monthly cash + optional feedstock; some effects description-only; calendar remains | HQ→Tech Tree; panel cycles projects; Start; budget cycle | E67 selected integration; I full tech tests | 12 Not run | OST-DES-001; verify each implemented effect separately from node completion |
| Distinct Commonwealth content/rolling stock → lore/art plan; Sprint37 | NML, tiny generated GRFs, catalog + identity-aware engine gate; runtime loading/industry and alias gaps | New-game NewGRF Settings; purchase/refit/cargo windows | I pack model; historical identity regression fixed; no current active-pack run | 13 **Blocked** in migrated save and incomplete fresh fixture | OST-CONT-001 + release provenance; 13 conceptual entries; verify actual loaded labels |
| Pipelines A–D/processing/onward delivery → Sprint42; production integration | player station facility commands/UI, input/output/monthly/lifecycle, PROD exist; vanilla aliases collapse materials | Owned station→Build production facility→recipe; input/output details | E67 real staged unload→conversion→load and actual save/reload | 14 Not run; no fully driven accepted chain | Use unhubbed vs hubbed world separately; 60 ore→30 steel example; content and hub gates |
| Federation external transport → Sprint35 | HTTP transport/despawn/materialization exist; natural entry closed | Operator topology; physical train driving once fixed | I real-process runner **manual dispatch**; domain/API historical evidence | 16 blocked for natural/recovery; no current multi-process run | OST-FED-001/003/PORT-001; independent transaction, identity/order/recovery work |
| Content admission/durable custody/return → Sprint12–16/22/29/35 | manifests, journals, protocol; physical durable recovery not demonstrated | Operator + two servers; player orders | I authority kits; weak conservation defaults/manual-return fallback in runner | 16 per-subcase Not run/Blocked | Exact physical quantities/global IDs/orders; faults/restarts/old-save reconciliation |
| Bespoke six-biome art and CST identity → art/lore plan; Sprint38 | procedural palettes/base sprites present; original asset sets not delivered | Normal zoom worlds, trains/structures | I palette classification only | 15 bespoke **Blocked** | Original content provenance + matched graphical captures |
| Guided tutorial and sprint documentation → Sprint23/28/34/36 | v0.4/v1.0/v1.1, GS9, 15 chapters/27 goals | Story Book and this checklist | H fixture verify; current v1.1 load smoke | Tutorial walkthrough Not run | Pins/goals do not prove fixture or result; retain historical numbering |
| LLM scenario generation / AI opponents / long-horizon UAT → AI spike | proposal; ordinary Script APIs/admin exist, OpenSpace wrappers/manifest runner/telemetry absent | No delivered player entry | I source API audit; no new AI run | None | Recovery gate then three separate research tracks in master plan |
| Branded title and moving megacity → requested showcase; intro engine | existing `opntitle.dat` loader and title game stable mechanism; custom scenario proposed | Opening menu background | I title code only | None | Small identity work independent; validated redistributable scenario later |

Source paths above are relative to `src/` unless otherwise stated. Full document
read inventory, immutable artifact hashes, checks and remote CI snapshots are in
[audit evidence](audit/2026-09-15/READING_INVENTORY.md). Sprint records remain
historical; this matrix supersedes their unqualified completion claims.
