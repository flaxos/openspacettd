class ValeRailway extends GSController {
	last_bits = -1;
	function Save() { return { last_bits = last_bits }; }
	function Load(version, data) { last_bits = -1; }
	function Element(page, type, reference, text) {
		local id = GSWorld0.FindElement(page, type, reference, text == null ? "" : text);
		return id != GSStoryPage.STORY_PAGE_ELEMENT_INVALID ? id : GSStoryPage.NewElement(page, type, reference, text);
	}
	function Text(page, text) { return Element(page, GSStoryPage.SPET_TEXT, 0, text); }
	function UI(company) {
		local page = GSWorld0.FindPage("The Vale Railway");
		if (page == GSStoryPage.STORY_PAGE_INVALID) page = GSStoryPage.New(company, "The Vale Railway");
		if (!GSStoryPage.IsValidStoryPage(page)) return null;
		Text(page, "You have founded Vale Railway in Aster Vale. Greybank, Stonecross and Merefield already have homes and roads. Stonecross Works needs a railway: bring silica from the quarry, then take its ballast to your yard beside Greybank.");
		Text(page, "Aster Vale is Phase 3 Frontier; its economic role is Frontier. Your charter permits one ballast works on its named plot. Founding assistance: GBP 750,000, imported 80 STEL, Materials I, an empty Pioneer with two SILC hoppers, the quarry and works, short platforms and depot stubs, and an empty Greybank yard. Both through railways are yours to build. Every SILC and BALL store starts empty.");
		Text(page, "Finish Quarry to Works input, give the Pioneer those two orders and start it. Collect BALL from the separate Works output platform and unload it at Greybank Yard. The yard stores your materials; warehouse freight pays no sale income. Rail fabrication starts off, so normal pioneer track is cash-built. With BALL and the finite imported STEL you may later enable fabrication.");
		Text(page, "No deadline or starvation loop applies. Empty input means send silica; full output means collect ballast. A stopped train, wrong orders or full buffer can be fixed. Two useful ballast arrivals separated by a return to the works establish the railway. Keep the company and continue free play.");
		local locations = ["Stonecross Quarry", "Stonecross Works", "Greybank Yard", "Greybank", "Stonecross", "Merefield"];
		for (local i = 0; i < locations.len(); ++i) {
			local tile = GSWorld0.GetLocation(i);
			if (tile != GSMap.TILE_INVALID) Element(page, GSStoryPage.SPET_LOCATION, tile, locations[i]);
		}
		local labels = ["Deliver 80 SILC and make 40 native ballast batches", "Bring 40 BALL home to Greybank Yard", "Return to the works and make another useful ballast delivery"];
		local goals = [];
		foreach (label in labels) {
			local id = GSWorld0.FindGoal(label);
			if (id == GSGoal.GOAL_INVALID) id = GSGoal.New(company, label, GSGoal.GT_NONE, 0);
			if (!GSGoal.IsValidGoal(id)) return null;
			goals.append(id); Element(page, GSStoryPage.SPET_GOAL, id, null);
		}
		return { page = page, goals = goals };
	}
	function Start() {
		while (!GSWorld0.IsActive()) this.Sleep(74);
		local company = GSWorld0.GetCompany();
		local ui = UI(company);
		if (ui != null) GSStoryPage.Show(ui.page);
		while (true) {
			local owner = GSWorld0.GetCompany();
			local valid = ui != null && GSStoryPage.IsValidStoryPage(ui.page);
			if (valid) foreach (id in ui.goals) if (!GSGoal.IsValidGoal(id)) valid = false;
			if (owner != company || !valid) {
				company = owner; ui = UI(company); last_bits = -1;
			}
			if (ui == null) { this.Sleep(74); continue; }
			local page = ui.page; local goals = ui.goals;
			local bits = GSWorld0.GetProgress();
			for (local i = 0; i < goals.len(); ++i) GSGoal.SetCompleted(goals[i], (bits & (1 << i)) != 0);
			GSGoal.SetProgress(goals[0], "SILC " + GSWorld0.GetCounter(0) + "/80; native batches " + GSWorld0.GetCounter(1) + "/40");
			GSGoal.SetProgress(goals[1], "BALL stored " + GSWorld0.GetCounter(2) + "/40");
			if ((bits & 16) != 0) Text(page, "Objective unavailable: " + GSWorld0.GetReason() + ". Achieved milestones and free play remain. Preserve this save; restart with the untouched founding opening if you want to try the chapter again.");
			if ((bits & 8) != 0) {
				local ending = GSWorld0.FindPage("Vale Railway is established");
				if (ending == GSStoryPage.STORY_PAGE_INVALID) ending = GSStoryPage.New(company, "Vale Railway is established");
				if (!GSStoryPage.IsValidStoryPage(ending)) { this.Sleep(74); continue; }
				Text(ending, "Stonecross Works has made ballast from your rail deliveries. Greybank Yard has received it, and your service has returned to the works and delivered again. Vale Railway is established. Keep your trains, materials and cash and continue playing. A passenger line between the settlements is a prospect for your next journey.");
				if (!(last_bits & 8)) GSStoryPage.Show(ending);
			}
			last_bits = bits;
			this.Sleep(74);
		}
	}
}
