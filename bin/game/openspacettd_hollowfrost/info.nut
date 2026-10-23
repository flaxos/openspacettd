class HollowfrostInfo extends GSInfo {
	function GetAuthor()      { return "OpenSpaceTTD Developers"; }
	function GetName()        { return "OpenSpaceTTD-Hollowfrost-Survey"; }
	function GetShortName()   { return "OSHF"; }
	function GetDescription() { return "The Hollowfrost Survey: rail rare earths and silica from frozen craters through a CST gate, grow crystal blanks on Kestrel Forge, and bring them home to be enriched under the ice-world sky."; }
	function GetVersion()     { return 1; }
	function MinVersionToLoad() { return 1; }
	function GetAPIVersion()  { return "16"; }
	function GetDate()        { return "2026-10-23"; }
	function CreateInstance() { return "HollowfrostSurvey"; }
}

RegisterGS(HollowfrostInfo());
