class OpenSpaceUATDemo extends GSController {
	initialized = false;
	service_built = false;
	sprint10_built = false;

	function Start();
	function Save();
	function Load(version, data);
	function CalculateRegions();
	function CalculateGateways(regions);
	function FindGatewayNear(nominal);
	function BuildAlphaDemonstrator(gateways);
	function BuildSprint10Fixtures(regions);
	function FindNearestTown(region, target);
	function AddText(page, text);
	function AddLocation(page, tile, text);
	function AddGoal(page, tile, text);
}

function OpenSpaceUATDemo::Save()
{
	return {
		initialized = this.initialized,
		service_built = this.service_built,
		sprint10_built = this.sprint10_built
	};
}

function OpenSpaceUATDemo::Load(version, data)
{
	/* Goals, Story Book pages, signs, and renamed towns are engine objects and
	 * are restored independently. Never create duplicate fixtures on load. */
	this.initialized = ("initialized" in data) ? data.initialized : true;
	this.service_built = ("service_built" in data) ? data.service_built : false;
	this.sprint10_built = ("sprint10_built" in data) ? data.sprint10_built : false;
	GSLog.Info("OpenSpaceTTD UAT Demo restored from savegame; Sprint 10 fixtures=" + this.sprint10_built + ".");
}

function OpenSpaceUATDemo::CalculateRegions()
{
	local size_x = GSMap.GetMapSizeX();
	local size_y = GSMap.GetMapSizeY();
	local world_count = 3;
	local padding = 2;
	local split_y = size_y >= size_x;
	local total_length = split_y ? size_y : size_x;
	local cross_length = split_y ? size_x : size_y;
	local start_coord = padding;
	local end_coord = total_length - 1 - padding;
	local available_span = end_coord - start_coord + 1;
	local buffer_width = min(size_x, size_y) / 16;
	buffer_width = max(4, buffer_width);

	while (2 * buffer_width + world_count * 8 > available_span && buffer_width > 2) {
		buffer_width /= 2;
	}

	local net_world_span = available_span - 2 * buffer_width;
	local base_world_span = net_world_span / world_count;
	local remainder = net_world_span % world_count;
	local cursor = start_coord;
	local regions = [];
	local metadata = [
		{ world = "World 1", phase = "Phase 1 Core", town = "Oaktree Core", role = "dense consumer and high-tech hub" },
		{ world = "World 2", phase = "Phase 2 Developed", town = "Merredin Industrial", role = "processing and intermodal backbone" },
		{ world = "World 3", phase = "Phase 3 Frontier", town = "Calyx Frontier", role = "primary extraction and heavy freight origin" }
	];

	for (local i = 0; i < world_count; i++) {
		local span = base_world_span + (i < remainder ? 1 : 0);
		local region = metadata[i];
		if (split_y) {
			region.min_x <- padding;
			region.max_x <- cross_length - 1 - padding;
			region.min_y <- cursor;
			region.max_y <- cursor + span - 1;
		} else {
			region.min_y <- padding;
			region.max_y <- cross_length - 1 - padding;
			region.min_x <- cursor;
			region.max_x <- cursor + span - 1;
		}
		region.center <- GSMap.GetTileIndex((region.min_x + region.max_x) / 2, (region.min_y + region.max_y) / 2);
		regions.append(region);
		cursor += span + buffer_width;
	}

	return regions;
}

function OpenSpaceUATDemo::CalculateGateways(regions)
{
	local gateways = [];
	for (local i = 0; i < regions.len() - 1; i++) {
		local first_region = regions[i];
		local second_region = regions[i + 1];
		local first_x;
		local first_y;
		local second_x;
		local second_y;
		local first_axis;
		local second_axis;

		if ((i & 1) == 0) {
			first_x = first_region.min_x + 3 * (first_region.max_x - first_region.min_x) / 4;
			first_y = first_region.min_y + (first_region.max_y - first_region.min_y) / 3;
			second_x = second_region.min_x + (second_region.max_x - second_region.min_x) / 4;
			second_y = second_region.min_y + 2 * (second_region.max_y - second_region.min_y) / 3;
			first_axis = "NE-SW";
			second_axis = "NW-SE";
		} else {
			first_x = first_region.min_x + 3 * (first_region.max_x - first_region.min_x) / 4;
			first_y = first_region.min_y + 2 * (first_region.max_y - first_region.min_y) / 3;
			second_x = second_region.min_x + (second_region.max_x - second_region.min_x) / 4;
			second_y = second_region.min_y + (second_region.max_y - second_region.min_y) / 3;
			first_axis = "NW-SE";
			second_axis = "NE-SW";
		}

		local first = this.FindGatewayNear(GSMap.GetTileIndex(first_x, first_y));
		local second = first != GSMap.TILE_INVALID ? GSTunnel.GetOtherTunnelEnd(first) : GSMap.TILE_INVALID;
		if (first == GSMap.TILE_INVALID || second == GSMap.TILE_INVALID) {
			first = GSMap.GetTileIndex(first_x, first_y);
			second = GSMap.GetTileIndex(second_x, second_y);
		}

		gateways.append({
			first = first,
			second = second,
			first_axis = first_axis,
			second_axis = second_axis
		});
	}
	return gateways;
}

function OpenSpaceUATDemo::FindGatewayNear(nominal)
{
	local nominal_x = GSMap.GetTileX(nominal);
	local nominal_y = GSMap.GetTileY(nominal);
	for (local radius = 0; radius <= 24; radius++) {
		for (local dy = -radius; dy <= radius; dy++) {
			for (local dx = -radius; dx <= radius; dx++) {
				if (max(abs(dx), abs(dy)) != radius) continue;
				local x = nominal_x + dx;
				local y = nominal_y + dy;
				if (x < 0 || y < 0 || x >= GSMap.GetMapSizeX() || y >= GSMap.GetMapSizeY()) continue;
				local tile = GSMap.GetTileIndex(x, y);
				if (!GSTunnel.IsTunnelTile(tile)) continue;
				if (GSTunnel.GetOtherTunnelEnd(tile) != GSMap.TILE_INVALID) return tile;
			}
		}
	}
	return GSMap.TILE_INVALID;
}

function OpenSpaceUATDemo::BuildAlphaDemonstrator(gateways)
{
	if (GSCompany.ResolveCompanyID(GSCompany.COMPANY_FIRST) == GSCompany.COMPANY_INVALID) return false;

	local alpha_a = gateways[0].first;
	local alpha_b = gateways[0].second;
	local ax = GSMap.GetTileX(alpha_a);
	local ay = GSMap.GetTileY(alpha_a);
	local bx = GSMap.GetTileX(alpha_b);
	local by = GSMap.GetTileY(alpha_b);
	local depot = GSMap.GetTileIndex(ax - 10, ay);
	local depot_front = GSMap.GetTileIndex(ax - 9, ay);
	local alpha_a_company_end = GSMap.GetTileIndex(ax - 2, ay);
	local alpha_a_neutral_lead = GSMap.GetTileIndex(ax - 1, ay);
	local alpha_b_neutral_lead = GSMap.GetTileIndex(bx, by + 1);
	local alpha_b_company_start = GSMap.GetTileIndex(bx, by + 2);
	local alpha_b_line_end = GSMap.GetTileIndex(bx, by + 6);
	local alpha_b_drag_end = GSMap.GetTileIndex(bx, by + 7);

	local company_mode = GSCompanyMode(GSCompany.COMPANY_FIRST);
	GSCompany.SetLoanAmount(GSCompany.GetMaxLoanAmount());

	local engine_id = null;
	local cheapest_price = 2147483647;
	local engines = GSEngineList(GSVehicle.VT_RAIL);
	foreach (candidate, unused in engines) {
		if (!GSEngine.IsBuildable(candidate) || GSEngine.IsWagon(candidate)) continue;
		local price = GSEngine.GetPrice(candidate);
		if (price < cheapest_price) {
			engine_id = candidate;
			cheapest_price = price;
		}
	}
	if (engine_id == null) {
		GSLog.Warning("Could not find a buildable locomotive for the Gateway Alpha demonstrator.");
		return false;
	}

	GSRail.SetCurrentRailType(GSEngine.GetRailType(engine_id));

	/* Prepare only the player-owned portions. The one-tile neutral leads and
	 * gateway heads are generated by the engine and deliberately retained. */
	for (local x = ax - 10; x <= ax - 2; x++) GSTile.DemolishTile(GSMap.GetTileIndex(x, ay));
	for (local y = by + 2; y <= by + 6; y++) GSTile.DemolishTile(GSMap.GetTileIndex(bx, y));
	GSTile.LevelTiles(alpha_a_company_end, depot);
	GSTile.LevelTiles(alpha_b_company_start, alpha_b_line_end);

	if (!GSRail.BuildRailDepot(depot, depot_front)) {
		GSLog.Warning("Could not build the Gateway Alpha demonstrator depot: " + GSError.GetLastErrorString());
		return false;
	}
	if (!GSRail.BuildRail(depot, depot_front, alpha_a_neutral_lead)) {
		GSLog.Warning("Could not build the Phase 1 demonstrator approach: " + GSError.GetLastErrorString());
		return false;
	}
	if (!GSRail.BuildRail(alpha_b_neutral_lead, alpha_b_company_start, alpha_b_drag_end)) {
		GSLog.Warning("Could not build the Phase 2 demonstrator turnback: " + GSError.GetLastErrorString());
		return false;
	}

	local vehicle = GSVehicle.BuildVehicle(depot, engine_id);
	if (!GSVehicle.IsValidVehicle(vehicle)) {
		GSLog.Warning("Could not build the Gateway Alpha demonstrator locomotive: " + GSError.GetLastErrorString());
		return false;
	}
	GSVehicle.SetName(vehicle, "UAT Wormhole Demonstrator");
	if (!GSVehicle.StartStopVehicle(vehicle)) {
		GSLog.Warning("Could not start the Gateway Alpha demonstrator locomotive: " + GSError.GetLastErrorString());
		return false;
	}

	GSSign.BuildSign(depot, "UAT Wormhole Demonstrator - start");
	GSSign.BuildSign(alpha_b_line_end, "UAT Wormhole Demonstrator - remote turnback");
	GSLog.Info("Gateway Alpha demonstrator is running between non-aligned, perpendicular portal heads.");
	return true;
}

function OpenSpaceUATDemo::BuildSprint10Fixtures(regions)
{
	if (GSCompany.ResolveCompanyID(GSCompany.COMPANY_FIRST) == GSCompany.COMPANY_INVALID) return false;

	local company_mode = GSCompanyMode(GSCompany.COMPANY_FIRST);
	GSCompany.SetLoanAmount(GSCompany.GetMaxLoanAmount());
	local railtypes = GSRailTypeList();
	local railtype = railtypes.Begin();
	if (railtypes.IsEnd()) {
		GSLog.Warning("Could not find a rail type for the Sprint 10 fixtures.");
		return false;
	}
	GSRail.SetCurrentRailType(railtype);

	/* Reuse an ordinary small airport as the Spaceport candidate. The station
	 * window added in Sprint 10 supplies designation, upgrades and telemetry. */
	local core = regions[0];
	local airport_type = GSAirport.AT_SMALL;
	local width = GSAirport.GetAirportWidth(airport_type);
	local height = GSAirport.GetAirportHeight(airport_type);
	local airport = GSMap.TILE_INVALID;
	for (local y = core.min_y + 10; y <= core.max_y - height - 10 && airport == GSMap.TILE_INVALID; y += 4) {
		for (local x = core.min_x + 10; x <= core.max_x - width - 10; x += 4) {
			local candidate = GSMap.GetTileIndex(x, y);
			if (!GSTile.IsBuildableRectangle(candidate, width, height)) continue;
			local level = GSTile.GetMinHeight(candidate);
			local flat = true;
			for (local ay = 0; ay < height && flat; ay++) {
				for (local ax = 0; ax < width; ax++) {
					local part = GSMap.GetTileIndex(x + ax, y + ay);
					if (GSTile.GetSlope(part) != GSTile.SLOPE_FLAT || GSTile.GetMinHeight(part) != level) {
						flat = false;
						break;
					}
				}
			}
			if (flat && GSAirport.BuildAirport(candidate, airport_type, GSStation.STATION_NEW)) airport = candidate;
			if (airport != GSMap.TILE_INVALID) break;
		}
	}
	if (airport == GSMap.TILE_INVALID) {
		GSLog.Warning("Could not find a flat buildable site for the Sprint 10 Spaceport candidate: " + GSError.GetLastErrorString());
		return false;
	}
	local airport_station = GSStation.GetStationID(airport);
	if (!GSStation.IsValidStation(airport_station)) return false;
	GSBaseStation.SetName(airport_station, "Phase 1 Spaceport Candidate");
	GSSign.BuildSign(airport, "SPRINT 10: Open station window - Designate Spaceport");

	/* Prepare a freight platform one tile inside the Phase 3 perimeter. The
	 * signed empty boundary tile remains available for the player's conduit. */
	local frontier = regions[2];
	local conduit = GSMap.TILE_INVALID;
	for (local y = frontier.min_y + 10; y <= frontier.max_y - 10; y++) {
		local candidate = GSMap.GetTileIndex(frontier.min_x, y);
		if (!GSTile.IsBuildableRectangle(candidate, 5, 1)) continue;
		local level = GSTile.GetMinHeight(candidate);
		local flat = true;
		for (local x = 0; x < 5; x++) {
			local part = GSMap.GetTileIndex(frontier.min_x + x, y);
			if (GSTile.GetSlope(part) != GSTile.SLOPE_FLAT || GSTile.GetMinHeight(part) != level) {
				flat = false;
				break;
			}
		}
		if (flat) {
			conduit = candidate;
			break;
		}
	}
	if (conduit == GSMap.TILE_INVALID) {
		GSLog.Warning("Could not find a flat Phase 3 boundary site for the Sprint 10 Edge Conduit fixture.");
		return false;
	}
	local lead = GSMap.GetTileIndex(GSMap.GetTileX(conduit) + 1, GSMap.GetTileY(conduit));
	local mineral_station = GSMap.GetTileIndex(GSMap.GetTileX(conduit) + 2, GSMap.GetTileY(conduit));
	if (!GSRail.BuildRailStation(mineral_station, GSRail.RAILTRACK_NE_SW, 1, 3, GSStation.STATION_NEW)) {
		GSLog.Warning("Could not build the Sprint 10 mineral receiving station: " + GSError.GetLastErrorString());
		return false;
	}
	GSRail.BuildRail(conduit, lead, mineral_station);
	local mineral_station_id = GSStation.GetStationID(mineral_station);
	if (GSStation.IsValidStation(mineral_station_id)) GSBaseStation.SetName(mineral_station_id, "Frontier Edge Minerals");
	GSSign.BuildSign(conduit, "SPRINT 10: Build Edge Conduit on this boundary tile");

	local page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "UAT - Planetary Operations");
	this.AddText(page, "Sprint 10 adapts existing OpenTTD controls. Spaceport controls appear only in an owned airport station window. The Edge Conduit tool is the final tunnel-style button on the Railway Construction toolbar.");
	this.AddLocation(page, airport, "Phase 1 Spaceport candidate airport");
	this.AddGoal(page, airport, "8. Open the airport station window and designate the Phase 1 Spaceport candidate. Verify Tier 1 status, world name, supplies, projected monthly cargo and total output.");
	this.AddGoal(page, airport, "9. Use the same station button to upgrade to Tier 2 and Tier 3. Verify the status changes and the Tier 3 button becomes disabled.");
	this.AddLocation(page, conduit, "Phase 3 signed Edge Conduit construction tile");
	this.AddGoal(page, conduit, "10. Select the final tunnel-style railway tool and build an Edge Conduit on the signed tile beside the void. Building one on an interior tile must explain that void adjacency is required.");
	this.AddGoal(page, conduit, "11. Use Land Area Information on the conduit and verify its world, mineral cargo, 100-unit Frontier monthly extraction and total extracted. Select it again with the conduit tool to remove it.");
	this.AddText(page, "Save and reload after building both structures. Their designation, tier and production counters must persist.");
	GSLog.Info("Sprint 10 fixtures ready: owned airport, Phase 3 boundary site, receiving rail station, and four acceptance goals.");
	GSStoryPage.Show(page);
	return true;
}

function OpenSpaceUATDemo::FindNearestTown(region, target)
{
	local towns = GSTownList();
	local nearest = null;
	local nearest_distance = 2147483647;

	for (local town = towns.Begin(); !towns.IsEnd(); town = towns.Next()) {
		local location = GSTown.GetLocation(town);
		local x = GSMap.GetTileX(location);
		local y = GSMap.GetTileY(location);
		if (x < region.min_x || x > region.max_x || y < region.min_y || y > region.max_y) continue;

		local distance = GSMap.DistanceManhattan(location, target);
		if (distance < nearest_distance) {
			nearest = town;
			nearest_distance = distance;
		}
	}

	return nearest;
}

function OpenSpaceUATDemo::AddText(page, text)
{
	GSStoryPage.NewElement(page, GSStoryPage.SPET_TEXT, 0, text);
}

function OpenSpaceUATDemo::AddLocation(page, tile, text)
{
	GSStoryPage.NewElement(page, GSStoryPage.SPET_LOCATION, tile, text);
}

function OpenSpaceUATDemo::AddGoal(page, tile, text)
{
	local goal = GSGoal.New(GSCompany.COMPANY_INVALID, text, GSGoal.GT_TILE, tile);
	if (GSGoal.IsValidGoal(goal)) GSStoryPage.NewElement(page, GSStoryPage.SPET_GOAL, goal, null);
}

function OpenSpaceUATDemo::Start()
{
	if (!this.initialized) {
		GSLog.Info("Creating OpenSpaceTTD three-world UAT fixtures.");
		local regions = this.CalculateRegions();
		local anchor_towns = 0;

		/* Give every phase an obvious anchor town without assuming a specific
		 * procedural seed beyond the deterministic demo configuration. */
		foreach (region in regions) {
			local town = this.FindNearestTown(region, region.center);
			if (town != null) {
				GSTown.SetName(town, region.town);
				GSTown.SetText(town, region.world + ": " + region.phase + " - " + region.role + ".");
				region.anchor <- GSTown.GetLocation(town);
				anchor_towns++;
			} else {
				region.anchor <- region.center;
			}
			GSSign.BuildSign(region.anchor, "UAT " + region.world + " - " + region.phase);
		}

		local gateway_sides = this.CalculateGateways(regions);
		for (local i = 0; i < gateway_sides.len(); i++) {
			local label = i == 0 ? "Gateway Alpha" : "Gateway Beta";
			GSSign.BuildSign(gateway_sides[i].first, label + " - World " + (i + 1) + " head (" + gateway_sides[i].first_axis + ")");
			GSSign.BuildSign(gateway_sides[i].second, label + " - World " + (i + 2) + " head (" + gateway_sides[i].second_axis + ")");
		}

		local overview = GSStoryPage.New(GSCompany.COMPANY_INVALID, "OpenSpaceTTD Three-World Demo");
		this.AddText(overview, "This deterministic UAT world is the Phase 1-2-3 vertical-slice foundation. Use Ctrl+Alt+1, Ctrl+Alt+2, and Ctrl+Alt+3 to jump between worlds.");
		foreach (region in regions) {
			this.AddLocation(overview, region.anchor, region.world + ": " + region.phase + " - " + region.role + ".");
		}
		this.AddText(overview, "Gateway Alpha links Phase 1 to Phase 2. Gateway Beta links Phase 2 to Phase 3. Each pair is spatially offset and rotates trains onto a different track axis, proving that gateways are not ordinary aligned tunnels. Void buffer bands isolate the three logical worlds.");

		local cargo_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "UAT - Data Crystals Rebranding");
		this.AddText(cargo_page, "All five checks must use Data Crystals and must not expose the legacy user-facing word Mail.");
		this.AddGoal(cargo_page, regions[0].anchor, "1. Graphs > Cargo Payment Rates lists Data Crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "2. Game Settings search shows Distribution mode for data crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "3. A town station acceptance/waiting list shows Data Crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "4. A train depot purchase list contains the Data Van wagon.");
		this.AddGoal(cargo_page, regions[0].anchor, "5. A road depot purchase list contains the MPS Data Courier.");

		local portal_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "UAT - Portal Gates");
		this.AddText(portal_page, "The generated gateways below are pre-linked reference pairs. Their heads are deep inside different worlds, are not aligned by X or Y coordinate, and use perpendicular local tracks. Use nearby player-owned rail to build a fresh pair with the portal button, then route a train through it.");
		this.AddLocation(portal_page, gateway_sides[0].first, "Gateway Alpha - Phase 1 side (NE-SW track)");
		this.AddLocation(portal_page, gateway_sides[0].second, "Gateway Alpha - Phase 2 side (NW-SE track)");
		this.AddLocation(portal_page, gateway_sides[1].first, "Gateway Beta - Phase 2 side (NW-SE track)");
		this.AddLocation(portal_page, gateway_sides[1].second, "Gateway Beta - Phase 3 side (NE-SW track)");
		this.AddGoal(portal_page, gateway_sides[0].first, "6. Build an unlinked portal gate, then link it to a second gate in another world.");
		this.AddGoal(portal_page, gateway_sides[0].first, "7. Route a train through Gateway Alpha and verify it emerges at the non-aligned Phase 2 head on the perpendicular track axis.");
		this.AddText(portal_page, "Portal Gates passed UAT 9.1. Spaceport and Edge Conduit GUI checks are on the Sprint 10 Planetary Operations page.");

		this.initialized = true;
		GSLog.Info("UAT fixtures ready: 3 worlds, " + anchor_towns + " anchor towns, 2 gateway pairs, and 7 acceptance goals.");
		GSStoryPage.Show(overview);
	}

	while (true) {
		local regions = this.CalculateRegions();
		if (!this.service_built) {
			this.service_built = this.BuildAlphaDemonstrator(this.CalculateGateways(regions));
		}
		if (!this.sprint10_built) this.sprint10_built = this.BuildSprint10Fixtures(regions);
		this.Sleep(74);
	}
}
