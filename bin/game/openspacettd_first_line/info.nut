class FirstLineInfo extends GSInfo {
	function GetAuthor() { return "OpenSpaceTTD Developers"; }
	function GetName() { return "OpenSpaceTTD-First-Permanent-Line"; }
	function GetShortName() { return "OSFL"; }
	function GetDescription() { return "A funded infrastructure commission: connect Clonclurry's ore to Merredin's works, build Mito's district, and keep the service running."; }
	function GetVersion() { return 1; }
	function MinVersionToLoad() { return 1; }
	function GetAPIVersion() { return "16"; }
	function GetDate() { return "2026-10-11"; }
	function CreateInstance() { return "FirstLine"; }
}
RegisterGS(FirstLineInfo());
