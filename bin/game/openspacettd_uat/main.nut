class OpenSpaceUATDemo extends GSController {
	initialized = false;
	service_built = false;
	sprint10_built = false;
	sprint28_built = false;
	sprint36_built = false;

	function Start();
	function Save();
	function Load(version, data);
	function CalculateRegions();
	function CalculateGateways(regions);
	function FindGatewayNear(nominal);
	function FindSpaceportSite(core);
	function FindConduitSite(frontier);
	function FindCSTStagingPad(developed);
	function FindBlueprintSampleSite(developed, cst_pad);
	function BuildAlphaDemonstrator(gateways);
	function BuildSprint10Fixtures(regions);
	function BuildSprint28Fixtures(regions);
	function BuildSprint36Fixtures(regions);
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
		sprint10_built = this.sprint10_built,
		sprint28_built = this.sprint28_built,
		sprint36_built = this.sprint36_built
	};
}

function OpenSpaceUATDemo::Load(version, data)
{
	/* Goals, Story Book pages, signs, and renamed towns are engine objects and
	 * are restored independently. Never create duplicate fixtures on load. */
	this.initialized = ("initialized" in data) ? data.initialized : true;
	this.service_built = ("service_built" in data) ? data.service_built : false;
	this.sprint10_built = ("sprint10_built" in data) ? data.sprint10_built : false;
	this.sprint28_built = ("sprint28_built" in data) ? data.sprint28_built : false;
	this.sprint36_built = ("sprint36_built" in data) ? data.sprint36_built : false;
	GSLog.Info("OpenSpaceTTD UAT Demo restored from savegame; Sprint 10 fixtures=" + this.sprint10_built + ", Sprint 28 fixtures=" + this.sprint28_built + ", Sprint 36 fixtures=" + this.sprint36_built + ".");
}

function OpenSpaceUATDemo::CalculateRegions()
{
	local size_x = GSMap.GetMapSizeX();
	local size_y = GSMap.GetMapSizeY();
	local world_count = (size_x >= 512 || size_y >= 512) ? 6 : 3;
	local padding = 2;
	local split_y = size_y >= size_x;
	local total_length = split_y ? size_y : size_x;
	local cross_length = split_y ? size_x : size_y;
	local start_coord = padding;
	local end_coord = total_length - 1 - padding;
	local available_span = end_coord - start_coord + 1;
	local buffer_width = min(size_x, size_y) / 16;
	buffer_width = max(4, buffer_width);

	local num_buffers = world_count - 1;
	while (num_buffers * buffer_width + world_count * 8 > available_span && buffer_width > 2) {
		buffer_width /= 2;
	}

	local net_world_span = available_span - num_buffers * buffer_width;
	local base_world_span = net_world_span / world_count;
	local remainder = net_world_span % world_count;
	local cursor = start_coord;
	local regions = [];
	local metadata = [
		{ world = "World 1", phase = "Phase 1 Core", town = "Oaktree Core", biome = "Temperate", role = "dense consumer, megacity, and central corporate headquarters campus" },
		{ world = "World 2", phase = "Phase 2 Developed", town = "Merredin Industrial", biome = "Arid Desert", role = "processing, interplanetary logistics hub, and fabrication staging" },
		{ world = "World 3", phase = "Phase 3 Frontier", town = "Calyx Frontier", biome = "Sub-Arctic", role = "primary extraction, cryogenic logistics, and heavy freight origin" },
		{ world = "World 4", phase = "Phase 4 Expansion", town = "Ignis Caldera", biome = "Volcanic", role = "uncolonized geothermal wilderness and rare mineral survey site" },
		{ world = "World 5", phase = "Phase 4 Expansion", town = "Verdant Canopy", biome = "Sub-Tropic", role = "uncolonized alien biosphere and bio-agricultural survey site" },
		{ world = "World 6", phase = "Phase 4 Expansion", town = "Pelagic Reach", biome = "Oceanic", role = "uncolonized archipelago frontier and maritime extraction basin" }
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

function OpenSpaceUATDemo::FindSpaceportSite(core)
{
	local airport_type = GSAirport.AT_SMALL;
	local width = GSAirport.GetAirportWidth(airport_type);
	local height = GSAirport.GetAirportHeight(airport_type);
	for (local y = core.min_y + 10; y <= core.max_y - height - 10; y += 4) {
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
			if (flat) return candidate;
		}
	}
	return GSMap.TILE_INVALID;
}

function OpenSpaceUATDemo::FindConduitSite(frontier)
{
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
		if (flat) return candidate;
	}
	return GSMap.TILE_INVALID;
}

function OpenSpaceUATDemo::FindCSTStagingPad(developed)
{
	for (local y = developed.min_y + 12; y <= developed.max_y - 14; y += 4) {
		for (local x = developed.min_x + 12; x <= developed.max_x - 18; x += 4) {
			local candidate = GSMap.GetTileIndex(x, y);
			if (!GSTile.IsBuildableRectangle(candidate, 16, 10)) continue;
			local level = GSTile.GetMinHeight(candidate);
			local flat = true;
			for (local ay = 0; ay < 10 && flat; ay++) {
				for (local ax = 0; ax < 16; ax++) {
					local part = GSMap.GetTileIndex(x + ax, y + ay);
					if (GSTile.GetSlope(part) != GSTile.SLOPE_FLAT || GSTile.GetMinHeight(part) != level) {
						flat = false;
						break;
					}
				}
			}
			if (flat) return candidate;
		}
	}
	return GSMap.TILE_INVALID;
}

function OpenSpaceUATDemo::FindBlueprintSampleSite(developed, cst_pad)
{
	for (local y = developed.min_y + 24; y <= developed.max_y - 10; y += 4) {
		for (local x = developed.min_x + 12; x <= developed.max_x - 16; x += 4) {
			local candidate = GSMap.GetTileIndex(x, y);
			if (cst_pad != GSMap.TILE_INVALID && GSMap.DistanceManhattan(candidate, cst_pad) < 14) continue;
			if (!GSTile.IsBuildableRectangle(candidate, 10, 4)) continue;
			local level = GSTile.GetMinHeight(candidate);
			local flat = true;
			for (local ay = 0; ay < 4 && flat; ay++) {
				for (local ax = 0; ax < 10; ax++) {
					local part = GSMap.GetTileIndex(x + ax, y + ay);
					if (GSTile.GetSlope(part) != GSTile.SLOPE_FLAT || GSTile.GetMinHeight(part) != level) {
						flat = false;
						break;
					}
				}
			}
			if (flat) return candidate;
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

	/* Prepare only the player-owned portions. The neutral leads and
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

	local core = regions[0];
	local airport = this.FindSpaceportSite(core);
	if (airport != GSMap.TILE_INVALID) {
		if (GSAirport.BuildAirport(airport, GSAirport.AT_SMALL, GSStation.STATION_NEW)) {
			local airport_station = GSStation.GetStationID(airport);
			if (GSStation.IsValidStation(airport_station)) GSBaseStation.SetName(airport_station, "Phase 1 Spaceport Candidate");
			GSSign.BuildSign(airport, "Phase 1 Spaceport Candidate - Designate Spaceport in station window");
		}
	} else {
		GSLog.Warning("Could not find a flat buildable site for the Spaceport candidate.");
	}

	local frontier = regions[2];
	local conduit = this.FindConduitSite(frontier);
	if (conduit != GSMap.TILE_INVALID) {
		local lead = GSMap.GetTileIndex(GSMap.GetTileX(conduit) + 1, GSMap.GetTileY(conduit));
		local mineral_station = GSMap.GetTileIndex(GSMap.GetTileX(conduit) + 2, GSMap.GetTileY(conduit));
		if (GSRail.BuildRailStation(mineral_station, GSRail.RAILTRACK_NE_SW, 1, 3, GSStation.STATION_NEW)) {
			GSRail.BuildRail(conduit, lead, mineral_station);
			local mineral_station_id = GSStation.GetStationID(mineral_station);
			if (GSStation.IsValidStation(mineral_station_id)) GSBaseStation.SetName(mineral_station_id, "Frontier Edge Minerals");
			GSSign.BuildSign(conduit, "Frontier Edge Minerals - Build Edge Conduit on void boundary");
		}
	} else {
		GSLog.Warning("Could not find a flat Phase 3 boundary site for the Edge Conduit fixture.");
	}

	GSLog.Info("Sprint 10 fixtures ready: owned airport and Frontier Edge Minerals receiving platform.");
	return true;
}

function OpenSpaceUATDemo::BuildSprint28Fixtures(regions)
{
	if (GSCompany.ResolveCompanyID(GSCompany.COMPANY_FIRST) == GSCompany.COMPANY_INVALID) return false;

	local company_mode = GSCompanyMode(GSCompany.COMPANY_FIRST);
	GSCompany.SetLoanAmount(GSCompany.GetMaxLoanAmount());

	local railtypes = GSRailTypeList();
	local railtype = railtypes.Begin();
	if (railtypes.IsEnd()) return false;
	GSRail.SetCurrentRailType(railtype);

	local developed = regions[1];
	local cst_pad = this.FindCSTStagingPad(developed);
	if (cst_pad != GSMap.TILE_INVALID) {
		local cst_end = GSMap.GetTileIndex(GSMap.GetTileX(cst_pad) + 15, GSMap.GetTileY(cst_pad) + 9);
		GSTile.LevelTiles(cst_pad, cst_end);
		GSSign.BuildSign(cst_pad, "UAT CST Prefab: Open Blueprint Library ('B') and stamp CST block here");
	}

	local bp_pad = this.FindBlueprintSampleSite(developed, cst_pad);
	if (bp_pad != GSMap.TILE_INVALID) {
		local bx = GSMap.GetTileX(bp_pad);
		local by = GSMap.GetTileY(bp_pad);
		local bp_end = GSMap.GetTileIndex(bx + 9, by + 3);
		GSTile.LevelTiles(bp_pad, bp_end);

		local t1_start = GSMap.GetTileIndex(bx + 1, by + 1);
		local t1_mid   = GSMap.GetTileIndex(bx + 4, by + 1);
		local t1_end   = GSMap.GetTileIndex(bx + 8, by + 1);
		local t2_start = GSMap.GetTileIndex(bx + 1, by + 2);
		local t2_mid   = GSMap.GetTileIndex(bx + 4, by + 2);
		local t2_end   = GSMap.GetTileIndex(bx + 8, by + 2);

		GSRail.BuildRail(t1_start, t1_mid, t1_end);
		GSRail.BuildRail(t2_start, t2_mid, t2_end);
		GSSign.BuildSign(bp_pad, "UAT Blueprint: Select 'Capture From Map' in Blueprint Library ('B')");
	}

	local core = regions[0];
	if ("anchor" in core) {
		local cx = GSMap.GetTileX(core.anchor);
		local cy = GSMap.GetTileY(core.anchor);
		GSSign.BuildSign(GSMap.GetTileIndex(cx + 2, cy), "UAT Megacity: Open Town/Megacity Overview to inspect demand tiers");
		GSSign.BuildSign(GSMap.GetTileIndex(cx, cy + 2), "UAT Federation: Open Map menu > 'Federation Authentication & Charters'");
		GSSign.BuildSign(GSMap.GetTileIndex(cx + 2, cy + 2), "UAT Trade Ledger: Open Map menu > 'Supply Chain & Trade Ledger'");
	}

	GSLog.Info("Sprint 28 fixtures ready: CST staging pad, Blueprint capture track layout, and navigation/governance signs.");
	return true;
}

function OpenSpaceUATDemo::BuildSprint36Fixtures(regions)
{
	if (GSCompany.ResolveCompanyID(GSCompany.COMPANY_FIRST) == GSCompany.COMPANY_INVALID) return false;

	local company_mode = GSCompanyMode(GSCompany.COMPANY_FIRST);

	/* 1. Corporate HQ fixtures on World 1 */
	local core = regions[0];
	local hq_tile = GSMap.GetTileIndex((core.min_x + core.max_x) / 2 + 10, (core.min_y + core.max_y) / 2 + 10);
	GSSign.BuildSign(hq_tile, "UAT Corporate HQ: Campus founded here. Open Map menu > 'Corporate Headquarters & Stockpiles'");

	/* 2. Logistics Hub & Stockpile fixtures on World 2 */
	if (regions.len() > 1) {
		local dev = regions[1];
		local hub_tile = GSMap.GetTileIndex((dev.min_x + dev.max_x) / 2 - 10, (dev.min_y + dev.max_y) / 2 - 10);
		GSSign.BuildSign(hub_tile, "UAT Logistics Hub: Merredin Planetary Logistics Hub. Buffer & Ingest train deliveries into local stockpile");
		local fab_tile = GSMap.GetTileIndex((dev.min_x + dev.max_x) / 2 - 8, (dev.min_y + dev.max_y) / 2 - 10);
		GSSign.BuildSign(fab_tile, "UAT Fabrication: Toggle In-Kind Fabrication in HQ window for 80% discount using local stockpile");
	}

	/* 3. Outpost survey fixtures on Expansion Worlds (Worlds 4, 5, 6) */
	if (regions.len() >= 6) {
		local w4 = regions[3];
		local w4_center = GSMap.GetTileIndex((w4.min_x + w4.max_x) / 2, (w4.min_y + w4.max_y) / 2);
		GSSign.BuildSign(w4_center, "UAT Outpost Survey: World 4 Volcanic Caldera. Use 'colonize_world 3' or Outpost tool to elevate to Frontier");

		local w5 = regions[4];
		local w5_center = GSMap.GetTileIndex((w5.min_x + w5.max_x) / 2, (w5.min_y + w5.max_y) / 2);
		GSSign.BuildSign(w5_center, "UAT Outpost Survey: World 5 Sub-Tropic Canopy. Survey site for biological extraction outpost");

		local w6 = regions[5];
		local w6_center = GSMap.GetTileIndex((w6.min_x + w6.max_x) / 2, (w6.min_y + w6.max_y) / 2);
		GSSign.BuildSign(w6_center, "UAT Outpost Survey: World 6 Oceanic Archipelago. Survey site for maritime resource outpost");
	}

	GSLog.Info("Sprint 36 fixtures ready: Corporate HQ, Logistics Hub, In-Kind Fabrication, and multi-world Outpost survey markers.");
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
	local goal = (tile != null && tile != GSMap.TILE_INVALID) ?
		GSGoal.New(GSCompany.COMPANY_INVALID, text, GSGoal.GT_TILE, tile) :
		GSGoal.New(GSCompany.COMPANY_INVALID, text, GSGoal.GT_NONE, 0);
	if (GSGoal.IsValidGoal(goal)) GSStoryPage.NewElement(page, GSStoryPage.SPET_GOAL, goal, null);
	return goal;
}

function OpenSpaceUATDemo::Start()
{
	if (!this.initialized) {
		GSLog.Info("Creating OpenSpaceTTD guided solo UAT fixtures and persistent Story Book.");
		local regions = this.CalculateRegions();
		local anchor_towns = 0;

		/* Establish canonical anchor towns for all 3 worlds. */
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
		local gateway_names = ["Gateway Alpha", "Gateway Beta", "Gateway Gamma", "Gateway Delta", "Gateway Epsilon"];
		for (local i = 0; i < gateway_sides.len(); i++) {
			local label = (i < gateway_names.len()) ? gateway_names[i] : ("Gateway " + (i + 1));
			GSSign.BuildSign(gateway_sides[i].first, label + " - World " + (i + 1) + " head (" + gateway_sides[i].first_axis + ")");
			GSSign.BuildSign(gateway_sides[i].second, label + " - World " + (i + 2) + " head (" + gateway_sides[i].second_axis + ")");
		}

		local spaceport_tile = this.FindSpaceportSite(regions[0]);
		local conduit_tile   = this.FindConduitSite(regions[2]);
		local cst_pad_tile   = this.FindCSTStagingPad(regions[1]);
		local bp_pad_tile    = this.FindBlueprintSampleSite(regions[1], cst_pad_tile);

		/* -------------------------------------------------------------
		 * CHAPTER 1: Overview & Planetary Navigation
		 * ------------------------------------------------------------- */
		local overview_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "1. Overview & Planetary Navigation");
		this.AddText(overview_page, "Welcome to the OpenSpaceTTD All-Feature Guided Solo UAT environment. This deterministic vertical slice connects up to six distinct planetary worlds across the void: World 1 (Phase 1 Core, Temperate), World 2 (Phase 2 Developed, Arid Desert), World 3 (Phase 3 Frontier, Sub-Arctic), World 4 (Phase 4 Expansion, Volcanic), World 5 (Phase 4 Expansion, Sub-Tropic), and World 6 (Phase 4 Expansion, Oceanic).");
		this.AddText(overview_page, "Use Ctrl+Alt+1 through Ctrl+Alt+6 or the Map dropdown menu to jump viewports instantly between worlds.");
		for (local r = 0; r < regions.len(); r++) {
			this.AddLocation(overview_page, regions[r].anchor, regions[r].world + " Anchor: " + regions[r].town + " (" + regions[r].phase + " - " + regions[r].biome + ")");
		}
		this.AddGoal(overview_page, regions[0].anchor, "1. Navigate all worlds using Ctrl+Alt+1..6 or the Map menu world jump buttons. Confirm distinct environmental biomes.");

		/* -------------------------------------------------------------
		 * CHAPTER 2: Monumental Portal Gates
		 * ------------------------------------------------------------- */
		local portal_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "2. Monumental Portal Gates");
		this.AddText(portal_page, "Monumental Portal Gates interlink planetary railway networks across the void. Pre-linked gateway pairs (Alpha 1<->2, Beta 2<->3, Gamma 3<->4, Delta 4<->5, Epsilon 5<->6) demonstrate arbitrary non-aligned spatial transitions with automatic 18-tile high-capacity terminals (holding lanes and path signals).");
		this.AddText(portal_page, "The 'UAT Wormhole Demonstrator' locomotive continuously cycles between perpendicular track axes on World 1 and World 2.");
		for (local g = 0; g < gateway_sides.len(); g++) {
			local glabel = (g < gateway_names.len()) ? gateway_names[g] : ("Gateway " + (g + 1));
			this.AddLocation(portal_page, gateway_sides[g].first, glabel + " - World " + (g + 1) + " head (" + gateway_sides[g].first_axis + ")");
			this.AddLocation(portal_page, gateway_sides[g].second, glabel + " - World " + (g + 2) + " head (" + gateway_sides[g].second_axis + ")");
		}
		this.AddGoal(portal_page, gateway_sides[0].first, "2. Build an unlinked portal gate with its automatic 18-tile two-lane terminal, then link it to a destination gate in another world.");
		this.AddGoal(portal_page, gateway_sides[0].first, "3. Run a portal consist through Gateway Alpha and verify it emerges smoothly at the non-aligned Phase 2 head on a perpendicular track axis.");

		/* -------------------------------------------------------------
		 * CHAPTER 3: Player Blueprints & CST Prefabs
		 * ------------------------------------------------------------- */
		local bp_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "3. Player Blueprints & CST Prefabs");
		this.AddText(bp_page, "Open the Blueprint Library by pressing 'B' or clicking the Blueprint icon on the Railway Construction toolbar. Test the 8 canonical CST Prefab Rail Blocks (using 'R' to rotate 90 degrees and 'F' to mirror/flip horizontally with RHD/LHD traffic invariance).");
		this.AddText(bp_page, "Capture custom player blueprints from the map using 'Capture From Map', save them with custom names and tags, and place replicas deterministically.");
		this.AddLocation(bp_page, cst_pad_tile != GSMap.TILE_INVALID ? cst_pad_tile : regions[1].center, "CST Prefab Staging Area (World 2: Merredin Industrial)");
		this.AddLocation(bp_page, bp_pad_tile != GSMap.TILE_INVALID ? bp_pad_tile : regions[1].center, "Custom Blueprint Capture Track Layout (World 2: Merredin Industrial)");
		this.AddGoal(bp_page, cst_pad_tile != GSMap.TILE_INVALID ? cst_pad_tile : regions[1].center, "4. Open Blueprint Library ('B'), select a canonical CST Prefab (e.g. CST Dual-Track Passing Siding or Mainline Double Straight), rotate/flip, and stamp it on the staging area.");
		this.AddGoal(bp_page, bp_pad_tile != GSMap.TILE_INVALID ? bp_pad_tile : regions[1].center, "5. Select 'Capture From Map' in the Blueprint Library, drag across the sample rail layout, save it to your local library, and place a replica.");

		/* -------------------------------------------------------------
		 * CHAPTER 4: Planetary Operations: Spaceports & Conduits
		 * ------------------------------------------------------------- */
		local ops_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "4. Planetary Operations: Spaceports & Conduits");
		this.AddText(ops_page, "Spaceport operations and void Edge Conduits expand planetary extraction and off-world logistics. Spaceport controls appear in owned airport station windows. Edge Conduits are built using the void-tunnel tool on the Railway Construction toolbar.");
		this.AddLocation(ops_page, spaceport_tile != GSMap.TILE_INVALID ? spaceport_tile : regions[0].center, "Phase 1 Spaceport candidate airport (World 1)");
		this.AddGoal(ops_page, spaceport_tile != GSMap.TILE_INVALID ? spaceport_tile : regions[0].center, "6. Open station window for the Phase 1 Spaceport candidate and designate Spaceport. Upgrade through Tier 2 and Tier 3, observing supply status and projected off-world trade cargo.");
		this.AddLocation(ops_page, conduit_tile != GSMap.TILE_INVALID ? conduit_tile : regions[2].center, "Phase 3 signed Edge Conduit construction tile (World 3)");
		this.AddGoal(ops_page, conduit_tile != GSMap.TILE_INVALID ? conduit_tile : regions[2].center, "7. Select the Edge Conduit tool on the rail toolbar and build on the signed Phase 3 void boundary tile. Verify Land Area Information reports 100 units/mo Frontier extraction and mineral cargo.");

		/* -------------------------------------------------------------
		 * CHAPTER 5: Megacity Demands & Freight Corridors
		 * ------------------------------------------------------------- */
		local mega_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "5. Megacity Demands & Freight Corridors");
		this.AddText(mega_page, "Metropolitan core worlds demand multi-tier commodity logistics: Tier 1 Sustenance (Food/Water), Tier 2 Expansion (Goods/Alloys), and Tier 3 Prosperity (Data Crystals/Valuables). Monthly cycles evaluate supply satisfaction and transition growth states: Starvation (0.0x), Subsistence (1.0x), Metropolitan Boom (1.5x), and HyperGrowth (2.0x).");
		this.AddText(mega_page, "The Freight Corridor Monitor tracks inter-world transport lanes, active transit volume, capacity utilization, and bottleneck escalation alerts.");
		this.AddLocation(mega_page, regions[0].anchor, "Oaktree Core (Metropolitan Megacity)");
		this.AddLocation(mega_page, gateway_sides[0].first, "Gateway Alpha Freight Corridor (Phase 1)");
		this.AddGoal(mega_page, regions[0].anchor, "8. Open Town window > 'Megacity' or Town menu > 'Megacity Overview'. Inspect Tier 1-3 demands and observe growth state transitions under monthly evaluation.");
		this.AddGoal(mega_page, gateway_sides[0].first, "9. Open Map dropdown > Freight Corridor Monitor. Inspect inter-world corridor transit volume, capacity utilization, and congestion bottleneck alerts.");

		/* -------------------------------------------------------------
		 * CHAPTER 6: Supply Chain Matrix & Federation Governance
		 * ------------------------------------------------------------- */
		local fed_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "6. Supply Chain Matrix & Federation Governance");
		this.AddText(fed_page, "Sprint 27 completed native player and operator UI surfaces. The Supply Chain Matrix & Trade Ledger window monitors Commonwealth macro phase flows, spaceport launch volume, conduit extraction, and bilateral commodity conservation audits.");
		this.AddText(fed_page, "The Federation Authentication & Charters window handles player session identity, corporate chartering, owner delegation, and world presence expansion.");
		this.AddLocation(fed_page, regions[0].anchor, "Federation Administrative Core (Oaktree Core)");
		this.AddGoal(fed_page, regions[0].anchor, "10. Open Map dropdown > Supply Chain & Trade Ledger. Inspect macro phase flows, spaceport/conduit infrastructure volume, and verify the CONSERVED commodity trade balance audit.");
		this.AddGoal(fed_page, regions[0].anchor, "11. Open Map dropdown > Federation Authentication & Charters. Authenticate player identity, charter a corporate entity, and expand world presence to World 1.");

		/* -------------------------------------------------------------
		 * CHAPTER 7: Commonwealth Data Crystals Rebranding
		 * ------------------------------------------------------------- */
		local cargo_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "7. Commonwealth Data Crystals Rebranding");
		this.AddText(cargo_page, "All user-facing references to the legacy mail cargo must display Data Crystals across economic, administrative, station, and vehicle surfaces.");
		this.AddLocation(cargo_page, regions[0].anchor, "Oaktree Core Station & Depot Area");
		this.AddGoal(cargo_page, regions[0].anchor, "12. Graphs > Cargo Payment Rates lists Data Crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "13. Game Settings search shows Distribution mode for data crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "14. Town station acceptance and waiting lists display Data Crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "15. Train depot purchase list contains the Data Van wagon.");
		this.AddGoal(cargo_page, regions[0].anchor, "16. Road depot purchase list contains the MPS Data Courier.");

		/* -------------------------------------------------------------
		 * CHAPTER 8: Phase 4 Colonisation & Frontier Outposts
		 * ------------------------------------------------------------- */
		local col_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "8. Phase 4 Colonisation & Frontier Outposts");
		this.AddText(col_page, "Uncolonised Expansion Worlds (Worlds 4, 5, 6) represent pristine planetary wildernesses featuring distinct alien biomes: Volcanic (World 4), Sub-Tropic (World 5), and Oceanic (World 6). Initially, town founding and processing facilities are restricted. Found colonial outposts using the Colonize Outpost tool or 'colonize_world <world_id>' console command to elevate the world to Phase 3 Frontier status, unlocking primary resource extraction, depots, and frontier towns.");
		if (regions.len() >= 6) {
			this.AddLocation(col_page, regions[3].anchor, "World 4 Anchor: Ignis Caldera (Volcanic Wilderness)");
			this.AddLocation(col_page, regions[4].anchor, "World 5 Anchor: Verdant Canopy (Sub-Tropic Wilderness)");
			this.AddLocation(col_page, regions[5].anchor, "World 6 Anchor: Pelagic Reach (Oceanic Wilderness)");
		}
		this.AddGoal(col_page, regions.len() >= 4 ? regions[3].anchor : regions[0].anchor, "17. Inspect uncolonised Expansion Worlds (Worlds 4, 5, 6) and verify environmental styling and pre-colonisation placement restrictions.");
		this.AddGoal(col_page, regions.len() >= 4 ? regions[3].anchor : regions[0].anchor, "18. Found a colonial outpost on an Expansion World to elevate it to Phase 3 Frontier status, unlocking primary extraction and settlement expansion.");

		/* -------------------------------------------------------------
		 * CHAPTER 9: Planetary Development Scoring & Phase Promotion
		 * ------------------------------------------------------------- */
		local dev_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "9. Planetary Development Scoring & Phase Promotion");
		this.AddText(dev_page, "Planetary worlds advance through 4 Commonwealth development phase tiers driven by logistics throughput and cargo delivery volume. Deliver cargo to stations across worlds to earn development score points (with interplanetary shipments earning significant premiums). When score thresholds are met (Phase 4->3: 100 pts, Phase 3->2: 5000 pts, Phase 2->1: 20000 pts), the world can be promoted via 'promote_world <world_id>' or the Planetary Operations window.");
		this.AddLocation(dev_page, regions[1].anchor, "World 2 Anchor: Merredin Industrial");
		this.AddGoal(dev_page, regions[1].anchor, "19. Deliver inter-world cargo across gateway pairs to accumulate planetary development score points.");
		this.AddGoal(dev_page, regions[1].anchor, "20. Promote a Frontier or Developed world to its next development tier when the score threshold is satisfied, unlocking higher technology tiers.");

		/* -------------------------------------------------------------
		 * CHAPTER 10: Corporate Headquarters Campus
		 * ------------------------------------------------------------- */
		local hq_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "10. Corporate Headquarters Campus");
		this.AddText(hq_page, "Player corporations that establish networks spanning at least 3 distinct world phases and possess 5,000,000 Cr in capital can establish a monumental Corporate Headquarters campus on a Phase 1 Core World. Open Map menu > 'Corporate Headquarters & Stockpiles' to manage the campus, inspect branch tiers (Regional Branch -> Planetary HQ -> Commonwealth HQ), and track macro holdings.");
		local hq_site = GSMap.GetTileIndex((regions[0].min_x + regions[0].max_x) / 2 + 10, (regions[0].min_y + regions[0].max_y) / 2 + 10);
		this.AddLocation(hq_page, hq_site, "Commonwealth Central HQ Campus (World 1: Oaktree Core)");
		this.AddGoal(hq_page, hq_site, "21. Open Map menu > 'Corporate Headquarters & Stockpiles' to inspect the established Commonwealth Central HQ campus on World 1.");
		this.AddGoal(hq_page, hq_site, "22. Advance headquarters tier through Planetary HQ and Commonwealth HQ to unlock corporate-wide bonuses.");

		/* -------------------------------------------------------------
		 * CHAPTER 11: Planetary Stockpiles & Logistics Hubs
		 * ------------------------------------------------------------- */
		local stock_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "11. Planetary Stockpiles & Logistics Hubs");
		this.AddText(stock_page, "Each planetary world maintains a dedicated company physical stockpile of core fabrication commodities: Ballast, Structural Metal, Wiring, Electronics, Superalloy, and Composites. Company Logistics Hubs built adjacent to freight stations buffer and ingest train deliveries directly into local planetary stockpiles. Configure minimum reserve floors at logistics hubs to prevent trains from depleting reserves below operational minimums.");
		local hub_site = (regions.len() > 1) ? GSMap.GetTileIndex((regions[1].min_x + regions[1].max_x) / 2 - 10, (regions[1].min_y + regions[1].max_y) / 2 - 10) : regions[0].anchor;
		this.AddLocation(stock_page, hub_site, "Merredin Planetary Logistics Hub (World 2)");
		this.AddGoal(stock_page, hub_site, "23. Open Corporate Headquarters > 'Planetary Stockpiles' tab and verify multi-world inventory levels across all 6 fabrication roles.");
		this.AddGoal(stock_page, hub_site, "24. Inspect the Merredin Planetary Logistics Hub on World 2, configure minimum reserve floors, and verify train stockpile ingestion.");

		/* -------------------------------------------------------------
		 * CHAPTER 12: In-Kind Fabrication & BOM Construction
		 * ------------------------------------------------------------- */
		local fab_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "12. In-Kind Fabrication & BOM Construction");
		this.AddText(fab_page, "Companies can toggle between standard commercial cash purchases and In-Kind Fabrication mode. When active, infrastructure construction (rail track, signals, depots) and vehicle manufacturing consume physical goods from the local planetary stockpile, applying a massive 80% cash discount.");
		local fab_site = (regions.len() > 1) ? GSMap.GetTileIndex((regions[1].min_x + regions[1].max_x) / 2 - 8, (regions[1].min_y + regions[1].max_y) / 2 - 10) : regions[0].anchor;
		this.AddLocation(fab_page, fab_site, "In-Kind Fabrication Track Staging (World 2)");
		this.AddGoal(fab_page, fab_site, "25. Toggle In-Kind Fabrication mode in the Corporate HQ window, construct rail infrastructure using local stockpile materials, and verify the 80% cash discount.");

		this.initialized = true;
		GSLog.Info("Guided solo UAT ready: " + regions.len() + " worlds, " + anchor_towns + " anchor towns, " + gateway_sides.len() + " gateway pairs, 12 Story Book chapters, and 25 acceptance goals.");
		GSStoryPage.Show(overview_page);
	}

	while (true) {
		local regions = this.CalculateRegions();
		if (!this.service_built) {
			this.service_built = this.BuildAlphaDemonstrator(this.CalculateGateways(regions));
		}
		if (!this.sprint10_built) this.sprint10_built = this.BuildSprint10Fixtures(regions);
		if (!this.sprint28_built) this.sprint28_built = this.BuildSprint28Fixtures(regions);
		if (!this.sprint36_built) this.sprint36_built = this.BuildSprint36Fixtures(regions);
		this.Sleep(74);
	}
}
