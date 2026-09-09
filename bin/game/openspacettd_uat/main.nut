class OpenSpaceUATDemo extends GSController {
	initialized = false;

	function Start();
	function Save();
	function Load(version, data);
	function CalculateRegions();
	function FindNearestTown(region, target);
	function AddText(page, text);
	function AddLocation(page, tile, text);
	function AddGoal(page, tile, text);
}

function OpenSpaceUATDemo::Save()
{
	return { initialized = this.initialized };
}

function OpenSpaceUATDemo::Load(version, data)
{
	/* Goals, Story Book pages, signs, and renamed towns are engine objects and
	 * are restored independently. Never create duplicate fixtures on load. */
	this.initialized = true;
	GSLog.Info("OpenSpaceTTD UAT Demo restored from savegame.");
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

		local split_y = GSMap.GetMapSizeY() >= GSMap.GetMapSizeX();
		local cross_center = (split_y ? GSMap.GetMapSizeX() : GSMap.GetMapSizeY()) / 2;
		local gateway_sides = [];
		for (local i = 0; i < regions.len() - 1; i++) {
			local first;
			local second;
			if (split_y) {
				first = GSMap.GetTileIndex(cross_center, regions[i].max_y - 1);
				second = GSMap.GetTileIndex(cross_center, regions[i + 1].min_y + 1);
			} else {
				first = GSMap.GetTileIndex(regions[i].max_x - 1, cross_center);
				second = GSMap.GetTileIndex(regions[i + 1].min_x + 1, cross_center);
			}
			gateway_sides.append([first, second]);
			local label = i == 0 ? "Gateway Alpha" : "Gateway Beta";
			GSSign.BuildSign(first, label + " - departure side");
			GSSign.BuildSign(second, label + " - arrival side");
		}

		local overview = GSStoryPage.New(GSCompany.COMPANY_INVALID, "OpenSpaceTTD Three-World Demo");
		this.AddText(overview, "This deterministic UAT world is the Phase 1-2-3 vertical-slice foundation. Use Ctrl+Alt+1, Ctrl+Alt+2, and Ctrl+Alt+3 to jump between worlds.");
		foreach (region in regions) {
			this.AddLocation(overview, region.anchor, region.world + ": " + region.phase + " - " + region.role + ".");
		}
		this.AddText(overview, "Gateway Alpha links Phase 1 to Phase 2. Gateway Beta links Phase 2 to Phase 3. Void buffer bands isolate the three logical worlds.");

		local cargo_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "UAT - Data Crystals Rebranding");
		this.AddText(cargo_page, "All five checks must use Data Crystals and must not expose the legacy user-facing word Mail.");
		this.AddGoal(cargo_page, regions[0].anchor, "1. Graphs > Cargo Payment Rates lists Data Crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "2. Game Settings search shows Distribution mode for data crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "3. A town station acceptance/waiting list shows Data Crystals.");
		this.AddGoal(cargo_page, regions[0].anchor, "4. A train depot purchase list contains the Data Van wagon.");
		this.AddGoal(cargo_page, regions[0].anchor, "5. A road depot purchase list contains the MPS Data Courier.");

		local portal_page = GSStoryPage.New(GSCompany.COMPANY_INVALID, "UAT - Portal Gates");
		this.AddText(portal_page, "The generated gateways below are pre-linked reference pairs. Use nearby player-owned rail to build a fresh pair with the portal button, then route a train through it.");
		this.AddLocation(portal_page, gateway_sides[0][0], "Gateway Alpha - Phase 1 side");
		this.AddLocation(portal_page, gateway_sides[0][1], "Gateway Alpha - Phase 2 side");
		this.AddLocation(portal_page, gateway_sides[1][0], "Gateway Beta - Phase 2 side");
		this.AddLocation(portal_page, gateway_sides[1][1], "Gateway Beta - Phase 3 side");
		this.AddGoal(portal_page, gateway_sides[0][0], "6. Build an unlinked portal gate, then link it to a second gate in another world.");
		this.AddGoal(portal_page, gateway_sides[0][0], "7. Route a train through the linked pair and verify intact emergence.");
		this.AddText(portal_page, "Spaceports and Edge Conduits remain backend-only in Sprint 9, so they are intentionally excluded from GUI acceptance goals.");

		this.initialized = true;
		GSLog.Info("UAT fixtures ready: 3 worlds, " + anchor_towns + " anchor towns, 2 gateway pairs, and 7 acceptance goals.");
		GSStoryPage.Show(overview);
	}

	while (true) this.Sleep(74);
}
