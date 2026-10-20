class FirstLine extends GSController {
	page = null;
	goals = null;
	house_markers = null;
	ending = false;
	last_bits = 0;
	function Save() { return { page = page, goals = goals, house_markers = house_markers, ending = ending, last_bits = last_bits }; }
	function Load(version, data) {
		page = data.page; goals = data.goals; house_markers = data.house_markers;
		ending = data.ending; last_bits = data.last_bits;
	}
	function Element(p, type, reference, text) {
		local existing = GSFirstLine.FindElement(p, type, reference, text == null ? "" : text);
		return existing != GSStoryPage.STORY_PAGE_ELEMENT_INVALID ? existing : GSStoryPage.NewElement(p, type, reference, text);
	}
	function Text(p, text) { return Element(p, GSStoryPage.SPET_TEXT, 0, text); }
	function Start() {
		while (!GSFirstLine.IsActive()) this.Sleep(74);
		local company = GSFirstLine.GetCompany();
		// Native UI identities survive even if a save interrupted a GS command.
		page = GSFirstLine.FindPage("The First Permanent Line");
		if (page == GSStoryPage.STORY_PAGE_INVALID) page = GSStoryPage.New(company, "The First Permanent Line");
			Text(page, "Your company has a name, two idle works and permission to join the public railway. On Mito, a building cooperative is waiting for the materials to open its district. Connect the suppliers, bring the works into service, and deliver the construction kit.");
			Text(page, "This is a funded infrastructure commission. Your opening cash, Materials I, prepared platforms, depot, empty starter train and Mito settlement registration are declared founding advantages. Every store starts empty. The public CST gates are inherited. Warehouse freight is company storage and pays no sale revenue.");
			Text(page, "Clonclurry IRON -> Merredin STEL; Clonclurry SILC -> Merredin BALL. Rail the products to your Mito warehouse. Commission residential works in Mito's town window: 160 STEL + 80 BALL + GBP 50,000. Four new houses must finish, then an export train must make a second useful deposit after returning to the works. Keep its recurring orders running.");
			Text(page, "You choose the connections, trains and orders. Capacity or Reserve is optional: either costs an extra 160 STEL + 80 BALL on Merredin plus GBP 50,000. Completing the district first is valid. FOOD is optional; the town remains stable while you experiment. No deadline or survival penalty applies.");
			local locations = ["Clonclurry iron supplier", "Clonclurry silica supplier", "Merredin steelworks", "Merredin ballast works", "Mito cooperative"];
			for (local i = 0; i < locations.len(); ++i) Element(page, GSStoryPage.SPET_LOCATION, GSFirstLine.GetLocation(i), locations[i]);
			goals = []; house_markers = [];
			local labels = ["Bring both native works into service", "Rail and commission Mito's 160 STEL / 80 BALL kit", "Complete four actual new district houses", "Return to the works and make a second useful Mito deposit"];
			foreach (label in labels) {
				local goal = GSFirstLine.FindGoal(label);
				if (goal == GSGoal.GOAL_INVALID) goal = GSGoal.New(company, label, GSGoal.GT_NONE, 0);
				goals.append(goal); Element(page, GSStoryPage.SPET_GOAL, goal, null);
			}
			GSStoryPage.Show(page);
		while (true) {
			local bits = GSFirstLine.GetProgress();
			if (bits != last_bits) {
				for (local i = 0; i < goals.len(); ++i) GSGoal.SetCompleted(goals[i], (bits & (1 << i)) != 0);
				if ((bits & 1) && !(last_bits & 1)) Text(page, "The idle works have a material service at last. Watch their input and output buffers in Facilities. A full output buffer needs collection; an empty input buffer needs the feeder.");
				if ((bits & 2) && !(last_bits & 2)) Text(page, "The cooperative's kit has been consumed on Mito. Its four building opportunities are paid for. Keep frontage clear while native construction proceeds; no second kit is needed.");
				if ((bits & 32) && !(last_bits & 32)) Text(page, "Mission evidence is invalid. Preserve this save and report the first unexpected event. Free play remains available; this run cannot receive verified completion.");
				last_bits = bits;
			}
			for (local i = house_markers.len(); i < 4; ++i) {
				local tile = GSFirstLine.GetHouse(i);
				if (tile == GSMap.TILE_INVALID) break;
				house_markers.append(Element(page, GSStoryPage.SPET_LOCATION, tile, "Actual commissioned house " + (i + 1)));
			}
			if ((bits & 16) && !ending) {
				local p = GSFirstLine.FindPage("A line people can rely on");
				if (p == GSStoryPage.STORY_PAGE_INVALID) p = GSStoryPage.New(company, "A line people can rely on");
				Text(p, "Four permanent houses stand on Mito. Your railway has returned to Merredin and made another useful delivery. The First Permanent Line is complete. Keep the company, trains and materials and continue playing.");
				Text(p, "Beyond today's public service lie other worlds. A future survey and your first independent connection are a prospect, not an unlock awarded here.");
				GSStoryPage.Show(p);
				ending = true;
			}
			this.Sleep(74);
		}
	}
}
