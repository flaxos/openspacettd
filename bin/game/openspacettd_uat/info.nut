class OpenSpaceUATDemoInfo extends GSInfo {
	function GetAuthor()      { return "OpenSpaceTTD Developers"; }
	/* Configuration keys are whitespace-delimited, so keep the registered
	 * script name human-readable without spaces. */
	function GetName()        { return "OpenSpaceTTD-UAT-Demo"; }
	function GetShortName()   { return "OSUD"; }
	function GetDescription() { return "Creates persistent UAT guidance, CST prefab & blueprint staging, Planetary Operations, Megacities, Freight Corridors, Federation governance, Corporate HQ, Logistics Hubs, and In-Kind Fabrication fixtures and map markers for the six-world OpenSpaceTTD vertical slice."; }
	function GetVersion()     { return 9; }
	function MinVersionToLoad() { return 1; }
	function GetAPIVersion()  { return "16"; }
	function GetDate()        { return "2026-09-15"; }
	function CreateInstance() { return "OpenSpaceUATDemo"; }
	function UseAsRandomAI()  { return false; }
}

RegisterGS(OpenSpaceUATDemoInfo());
