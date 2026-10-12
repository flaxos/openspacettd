class ValeInfo extends GSInfo {
	function GetAuthor() { return "OpenSpaceTTD Developers"; }
	function GetName() { return "OpenSpaceTTD-Vale-Railway"; }
	function GetShortName() { return "OSVL"; }
	function GetDescription() { return "Found a useful railway in Aster Vale: connect the quarry, make ballast and bring it home to Greybank."; }
	function GetVersion() { return 1; }
	function MinVersionToLoad() { return 1; }
	function GetAPIVersion() { return "16"; }
	function GetDate() { return "2026-10-12"; }
	function CreateInstance() { return "ValeRailway"; }
}
RegisterGS(ValeInfo());
