# Player-built resource economy

New games default to **Player-built** economy. Select **Classic** in the new-game
window to use the original rules. Existing saves without the `RSRC` chunk and
existing authored demo configurations remain Classic.

## Player flow

1. Generate a new map. Starter density defaults to Low. Only basic primary
   industries start operating; processing industries must be built by companies.
   No spontaneous industries open later. Multiple industries per town starts
   enabled in this mode, so town identity does not prevent resource development.
2. Open **Industries → Fund new industry → Survey / discovered sites**.
3. Choose **Survey area** and click the top-left of a 16×16 square. The window
   quotes the price: 1% of the current base primary-industry construction price,
   rounded up. Empty surveys cost money too; fully covered areas are free to repeat.
4. Select a discovered site, choose **Build at site**, and click inside its outline.
   Blue outlines are available; red outlines are occupied. The whole industry
   footprint must fit. Terrain, climate, world phase, NewGRF callbacks and other
   native placement rules still apply. Construction and surveying are separate costs.
5. Research Materials II in the Corporate HQ to reveal copper and silica/sand;
   Materials III reveals rare minerals. Previously surveyed areas reveal these
   resources automatically. Cash-funded research avoids a dependency on locked
   resources. Basic agriculture, forestry, fishing, coal, oil, iron and stone need
   no new technology. Unrecognised primary resource types use this basic tier.

Surveys belong to companies and reserve no land. Other companies need their own
survey; the first successful builder occupies the site. Company acquisitions merge
survey coverage and deletion removes it. Existing production/closure mechanics
remain. Closure or demolition frees a site for rebuilding. There is no new depletion,
claim trading, survey train, or automatic existing-save conversion.

The survey area must remain within one logical world and avoid void. Discovery
uses a site's anchor; the discovered site can extend beyond the surveyed square.
Hidden sites are generated from native placement probes, with at least one attempt
per enabled, fundable primary type and weighted targets of four undeveloped sites
per configured starter slot. Terrain, conflicting industries, world policy and
limited space can reduce actual counts. Shortfalls are logged as `Resource sites`.
Generation attempts are bounded. An enabled industry pack can still refuse a
particular construction through its own callbacks.

## Implementation and interfaces

`ResourceSiteManager` in `src/portal/resource_sites.*` owns stable GRFID/local-ID
site identities, rectangles, world IDs, occupancy and per-company survey squares.
Vanilla types use their native IDs. Extractive/organic industry flags and tropical
lumber mills define primary industries; cargo labels determine advanced tiers.
No runtime cargo-slot numbers or translated names define resource policy.

`Commands::SurveyResources` is an ordinary company command. Native BuildIndustry
preflight validates site, discovery, research and full layout before any mutation.
UI and AI/script calls share these handlers; failed estimates and failed commands
consume neither sites nor money. Privileged scenario/editor construction may
register operating industries without a player survey. The map remains one global
map with immutable logical regions.

The named-table `RSRC` chunk saves mode, sites and coverage; reset defaults to
Classic, and post-load validation drops invalid content/world/company references
and clears stale occupancy. Company research stays in `TECH`. Native map transfer
carries the same save chunks to clients. Private discoveries are filtered by the
normal UI and script API, not encrypted against modified clients.

ScriptIndustryType adds `SurveyResources(tile)` and
`GetDiscoveredResourceSites(industry_type)` (anchor → occupied). Existing
BuildIndustry works at eligible surveyed sites; old random ProspectIndustry is
unavailable in Player-built mode. The read-only `resource_sites` console command
reports mode and counts without disclosing hidden site positions.

## Verification and reproduction

```sh
ninja -C build -j4
./build/openttd_test '[resource-sites]'
python3 scripts/test_command_authority_network.py build/openttd_test --resource-surveys
python3 scripts/test_resource_generation.py
python3 scripts/generate_connected_economy.py --resource-surveys --verify --steps 160 --output build/resource-economy-acceptance
```

The generation runner uses fresh native maps, checks defaults, Funding only and
Classic modes, joins two SDL dummy clients, and compares resource counts after a
fresh-process reload. The separate command relay exercises paid company surveys,
competing builders and save/reload in three independent native engine processes;
it does not itself exercise the join handshake.

The resource variant of the existing connected fixture uses authored sites and
normal survey/construction handlers, cash-funded Materials research, and the
existing trains, production facilities, warehouses and consumers. It is **not**
evidence for random site generation or low-budget starting balance. Its acceptance
requires repeated delivery on every service, 24 months of conservation, reload,
starvation/recovery, fabrication, research consumption and genuine house/population
growth from cold start. The original showcase retains its stricter three-houses-in-
ten-minutes presentation target; that is not a resource-system requirement.
Research bonus output is included in the optional audit with its actual monthly
rounding; gameplay production is unchanged.

Automated GUI tests instantiate the industry and survey windows and exercise the
entry point. Human graphical acceptance, survey-price balance, resource abundance,
and third-party NewGRF coverage remain separate from these automated checks.
See `demo/UAT-RESULTS.md` for the recorded verification status.
