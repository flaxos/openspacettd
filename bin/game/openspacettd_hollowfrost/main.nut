/* The Hollowfrost Survey. Progress is read from native cargo monitors and the
 * Kestrel Forge stockpile only; the script never moves cargo or builds track.
 * Milestone bits are set before their commands so a reload never repeats a reward. */
class HollowfrostSurvey extends GSController {
	company = -1;
	ids = null;
	cargo = null;
	page = -1;
	goals = null;
	offer = -1;
	buttons = null;
	sand = 0;
	rare = 0;
	blanks = 0;
	target = 40;
	/* 1 ore, 2 blanks, 4 crystals, 8 offer shown, 16 contract signed, 32 priority, 64 ore news, 128 aurora news, 256 first crystal news. */
	bits = 0;

	constructor() {
		this.ids = {};
		this.cargo = {};
		this.goals = [];
		this.buttons = [];
	}

	function Save() {
		return { company = this.company, ids = this.ids, page = this.page, goals = this.goals, offer = this.offer,
			buttons = this.buttons, sand = this.sand, rare = this.rare, blanks = this.blanks, target = this.target, bits = this.bits };
	}

	function Load(version, data) {
		this.company = data.company; this.ids = data.ids; this.page = data.page; this.goals = data.goals;
		this.offer = data.offer; this.buttons = data.buttons; this.sand = data.sand; this.rare = data.rare;
		this.blanks = data.blanks; this.target = data.target; this.bits = data.bits;
	}

	function Text(page, text) { return GSStoryPage.NewElement(page, GSStoryPage.SPET_TEXT, 0, text); }

	/* Find the charter company, the four works and the Institute warehouse by their cargo and world. */
	function Discover() {
		if (GSPlanet.GetWorldCount() != 2 || GSPlanet.GetName(0) != "Hollowfrost" || GSPlanet.GetName(1) != "Kestrel Forge") return false;
		foreach (id, _ in GSCargoList()) this.cargo[GSCargo.GetCargoLabel(id)] <- id;
		foreach (label in ["SAND", "RARE", "BCRY", "QCRY"]) if (!(label in this.cargo)) return false;
		if (this.company == -1) {
			local first = GSCompany.ResolveCompanyID(GSCompany.COMPANY_FIRST);
			if (first == GSCompany.COMPANY_INVALID) return false;
			this.company = first;
		}
		if ("institute" in this.ids) return true;
		local found = {};
		foreach (industry, _ in GSIndustryList()) {
			local world = GSPlanet.GetTileWorld(GSIndustry.GetLocation(industry));
			local produces = GSCargoList_IndustryProducing(industry);
			if (world == 0 && produces.HasItem(this.cargo["RARE"])) found.mine <- industry;
			if (world == 0 && produces.HasItem(this.cargo["SAND"])) found.silica <- industry;
			if (world == 0 && produces.HasItem(this.cargo["QCRY"])) found.observatory <- industry;
			if (world == 1 && produces.HasItem(this.cargo["BCRY"])) found.fab <- industry;
		}
		foreach (station, _ in GSStationList(GSStation.STATION_TRAIN)) {
			if (GSBaseStation.GetName(station) == "Kestrel Survey Institute") found.institute <- station;
		}
		foreach (key in ["mine", "silica", "observatory", "fab", "institute"]) if (!(key in found)) return false;
		this.ids = found;
		return true;
	}

	function Brief() {
		this.page = GSStoryPage.New(this.company, "The Hollowfrost Survey");
		local p = this.page;
		Text(p, "Hollowfrost is a frozen frontier pocked by ancient impacts. Rare earths are mined on the floor of The Hollow, the great crater at the west end of the surveyed line. Silica glitters on the Glass Flats. The old Rimehold Observatory sits beneath the rim of Rimehold Crater, where the cold, dark sky is perfect for enriching quantum crystals.");
		Text(p, "Through the CST gate lies Kestrel Forge, a black basalt world of mesas and ash flats. Its monocrystal fab grows blank data crystals from two parts silica and one part rare earths, but only the observatory on Hollowfrost can enrich them. The Commonwealth Survey Institute at Kestrel needs quantum crystals to chart the next gate.");
		Text(p, "Your charter: GBP 2,000,000, Materials I to IV and Traction I licences, the mine, silica pit, fab and observatory (their imported construction kits are spent), five platforms on the surveyed line, a depot at each end, an empty Institute warehouse and, in the Hollow Mine depot, an empty survey train: a Vulcan with two silica hoppers, one rare-earth hopper, two blank-crystal vaults and one quantum-crystal vault. The surveyed line runs from The Breach to the gate, and on from the gate to the Institute; you lay the rail. Every store starts empty.");
		Text(p, "Rail RARE and SAND through the gate to Kestrel Fab. Carry the blank crystals (BCRY) back through the gate to Rimehold Observatory in CST vault cars, then take its quantum crystals (QCRY) to the Kestrel Survey Institute, which stores them. The Institute warehouse stores everything a train unloads there, so arrive with only quantum crystals: unload the ore at the fab without loading, visit the Institute, then load the blanks at the fab on the way home. Deliveries to the fab and the observatory pay freight; warehouse storage does not. A second train on the same line needs passing loops and signals.");
		Text(p, "There is no deadline. An empty fab input means send ore; a full output means collect crystals. When the survey ends, keep playing.");
		foreach (row in [[GSIndustry.GetLocation(this.ids.mine), "Hollow Mine"], [GSIndustry.GetLocation(this.ids.silica), "Glass Flats silica"],
				[GSIndustry.GetLocation(this.ids.observatory), "Rimehold Observatory"], [GSIndustry.GetLocation(this.ids.fab), "Kestrel Monocrystal Fab"],
				[GSStation.GetLocation(this.ids.institute), "Kestrel Survey Institute"]]) {
			GSStoryPage.NewElement(p, GSStoryPage.SPET_LOCATION, row[0], row[1]);
		}
		this.goals = [
			GSGoal.New(this.company, "Deliver 180 silica and 90 rare earths to Kestrel Fab", GSGoal.GT_INDUSTRY, this.ids.fab),
			GSGoal.New(this.company, "Deliver 60 blank data crystals to Rimehold Observatory", GSGoal.GT_INDUSTRY, this.ids.observatory),
			GSGoal.New(this.company, "Store 40 quantum crystals at Kestrel Survey Institute", GSGoal.GT_TILE, GSStation.GetLocation(this.ids.institute)),
		];
		foreach (goal in this.goals) GSStoryPage.NewElement(p, GSStoryPage.SPET_GOAL, goal, null);
		GSStoryPage.Show(p);
	}

	function Offer() {
		this.bits = this.bits | 8;
		this.offer = GSStoryPage.New(this.company, "A contract from the Survey Institute");
		Text(this.offer, "Hollowfrost ore is reaching Kestrel Fab. The Survey Institute offers two contracts for the quantum crystals. You can choose once; until you do, the standard contract applies.");
		this.buttons = [
			GSStoryPage.NewElement(this.offer, GSStoryPage.SPET_BUTTON_PUSH, GSStoryPage.MakePushButtonReference(GSStoryPage.SPBC_LIGHT_BLUE, GSStoryPage.SPBF_NONE), "Standard: store 40 crystals, GBP 250,000 bonus"),
			GSStoryPage.NewElement(this.offer, GSStoryPage.SPET_BUTTON_PUSH, GSStoryPage.MakePushButtonReference(GSStoryPage.SPBC_ORANGE, GSStoryPage.SPBF_NONE), "Priority: store 100 crystals, GBP 1,000,000 bonus"),
		];
		GSStoryPage.Show(this.offer);
	}

	function HandleEvents() {
		while (GSEventController.IsEventWaiting()) {
			local event = GSEventController.GetNextEvent();
			if (event.GetEventType() != GSEvent.ET_STORYPAGE_BUTTON_CLICK) continue;
			local click = GSEventStoryPageButtonClick.Convert(event);
			if (click.GetStoryPageID() != this.offer || this.buttons.len() != 2 || (this.bits & 20) != 0) continue;
			local priority = click.GetElementID() == this.buttons[1];
			if (!priority && click.GetElementID() != this.buttons[0]) continue;
			this.bits = this.bits | 16 | (priority ? 32 : 0);
			this.target = priority ? 100 : 40;
			GSGoal.SetText(this.goals[2], "Store " + this.target + " quantum crystals at Kestrel Survey Institute");
			foreach (button in this.buttons) GSStoryPage.RemoveElement(button);
			Text(this.offer, priority ? "Priority contract signed: 100 quantum crystals for a GBP 1,000,000 bonus." : "Standard contract signed: 40 quantum crystals for a GBP 250,000 bonus.");
		}
	}

	function Delta(amount) { return amount > 0 ? amount : 0; }

	function Update() {
		this.sand += Delta(GSCargoMonitor.GetIndustryDeliveryAmount(this.company, this.cargo["SAND"], this.ids.fab, true));
		this.rare += Delta(GSCargoMonitor.GetIndustryDeliveryAmount(this.company, this.cargo["RARE"], this.ids.fab, true));
		this.blanks += Delta(GSCargoMonitor.GetIndustryDeliveryAmount(this.company, this.cargo["BCRY"], this.ids.observatory, true));
		local stored = Delta(GSPlanet.GetStock(1, this.company, this.cargo["QCRY"]));
		if ((this.bits & 1) == 0) GSGoal.SetProgress(this.goals[0], "SAND " + min(this.sand, 180) + "/180, RARE " + min(this.rare, 90) + "/90");
		if ((this.bits & 2) == 0) GSGoal.SetProgress(this.goals[1], "BCRY " + min(this.blanks, 60) + "/60");
		if ((this.bits & 4) == 0) GSGoal.SetProgress(this.goals[2], "QCRY " + min(stored, this.target) + "/" + this.target);

		if ((this.bits & 64) == 0 && this.sand + this.rare > 0) {
			this.bits = this.bits | 64;
			GSNews.Create(GSNews.NT_GENERAL, "Hollowfrost ore has crossed the CST gate. Kestrel Fab can now grow blank data crystals.", this.company, GSNews.NR_INDUSTRY, this.ids.fab);
		}
		if ((this.bits & 1) == 0 && this.sand >= 180 && this.rare >= 90) {
			this.bits = this.bits | 1;
			GSGoal.SetProgress(this.goals[0], "Done");
			GSGoal.SetCompleted(this.goals[0], true);
		}
		if ((this.bits & 1) != 0 && (this.bits & 12) == 0) Offer();
		if ((this.bits & 128) == 0 && this.blanks > 0) {
			this.bits = this.bits | 128;
			GSNews.Create(GSNews.NT_GENERAL, "Aurora over Rimehold! The observatory has begun enriching blank crystals into quantum crystals.", this.company, GSNews.NR_INDUSTRY, this.ids.observatory);
		}
		if ((this.bits & 2) == 0 && this.blanks >= 60) {
			this.bits = this.bits | 2;
			GSGoal.SetProgress(this.goals[1], "Done");
			GSGoal.SetCompleted(this.goals[1], true);
		}
		if ((this.bits & 256) == 0 && stored > 0) {
			this.bits = this.bits | 256;
			GSNews.Create(GSNews.NT_GENERAL, "The first quantum crystals are in the Kestrel Survey Institute warehouse.", this.company, GSNews.NR_TILE, GSStation.GetLocation(this.ids.institute));
		}
		if ((this.bits & 4) == 0 && (this.bits & 3) == 3 && stored >= this.target) Finish();
	}

	function Finish() {
		this.bits = this.bits | 4;
		local bonus = (this.bits & 32) != 0 ? 1000000 : 250000;
		GSGoal.SetProgress(this.goals[2], "Done");
		GSGoal.SetCompleted(this.goals[2], true);
		GSCompany.ChangeBankBalance(this.company, bonus, GSCompany.EXPENSES_OTHER, GSMap.TILE_INVALID);
		local ending = GSStoryPage.New(this.company, "The survey is complete");
		Text(ending, "The Survey Institute has its quantum crystals. Their lattices carry the faint ringing that only Hollowfrost's frozen sky leaves behind, and the Institute's navigators are already charting the next gate. Your GBP " + bonus + " bonus has been paid. Keep the company, trains and stores, and play on.");
		GSStoryPage.Show(ending);
		GSNews.Create(GSNews.NT_GENERAL, "Hollowfrost Survey complete: the Survey Institute has its quantum crystals.", this.company, GSNews.NR_NONE, 0);
	}

	function Start() {
		while (!Discover()) this.Sleep(74);
		if (this.page == -1) Brief();
		while (true) {
			HandleEvents();
			Update();
			this.Sleep(74);
		}
	}
}
