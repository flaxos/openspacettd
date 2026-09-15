/** @file test_blueprint_storage.cpp Blueprint storage regressions using a private temporary library. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../blueprint/blueprint_manager.h"
#include "../fileio_func.h"
#include <filesystem>
#include <fstream>
#include <chrono>


extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;

class PrivateBlueprintLibrary {
public:
	std::filesystem::path root;
	std::string old_personal;
	std::vector<Searchpath> old_valid;
	PrivateBlueprintLibrary()
	{
		old_personal = _searchpaths[Searchpath::PersonalDir];
		old_valid = _valid_searchpaths;
		root = std::filesystem::temp_directory_path() / ("ost-bp-storage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(root / "blueprint");
		_searchpaths[Searchpath::PersonalDir] = root.string() + "/";
		_valid_searchpaths = {Searchpath::PersonalDir};
		BlueprintManager::Reset();
	}
	~PrivateBlueprintLibrary()
	{
		BlueprintManager::Reset();
		_valid_searchpaths = std::move(old_valid);
		_searchpaths[Searchpath::PersonalDir] = std::move(old_personal);
		std::error_code ec;
		std::filesystem::remove_all(root, ec);
	}
	std::filesystem::path dir() const { return root / "blueprint"; }
};

static Blueprint StorageBlueprint(const std::string &name)
{
	Blueprint bp;
	bp.name = name;
	bp.width = bp.height = 1;
	BlueprintTile tile;
	tile.type = BlueprintTileType::Track;
	tile.trackbits = TrackBits{Track::X};
	bp.tiles.push_back(tile);
	return bp;
}

static std::string FileBytes(const std::filesystem::path &path)
{
	std::ifstream in(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST_CASE("Blueprint file export and import preserve content and reject existing destination", "[blueprint][storage]")
{
	PrivateBlueprintLibrary fixture;
	auto bp = StorageBlueprint("Round Trip");
	auto export_path = fixture.root / "shared.json";
	std::string error;
	REQUIRE(BlueprintManager::ExportToFile(bp, export_path.string(), &error));
	CHECK(FileBytes(export_path) == bp.ToJson());
	CHECK_FALSE(BlueprintManager::ExportToFile(bp, export_path.string(), &error));
	CHECK_FALSE(error.empty());
	CHECK(FileBytes(export_path) == bp.ToJson());
	REQUIRE(BlueprintManager::ImportFromFile(export_path.string(), &error));
	CHECK(BlueprintManager::GetBlueprints().back().name == bp.name);
	CHECK_FALSE(BlueprintManager::ImportFromFile(export_path.string(), &error));
	CHECK_FALSE(error.empty());
	CHECK(BlueprintManager::GetBlueprints().back().name == bp.name);
}

TEST_CASE("CST revision updates preserve imported older layouts with the same name", "[blueprint][storage][cst_prefab]")
{
	PrivateBlueprintLibrary fixture;
	BlueprintManager::Initialize();
	const std::string name = "CST High-Speed 3-Way Wye Junction";
	const Blueprint *builtin = BlueprintManager::FindBuiltin(name);
	REQUIRE(builtin != nullptr);
	REQUIRE(builtin->layout_revision == 2);
	const std::string current_layout = builtin->ToJson();
	Blueprint legacy = *builtin;
	legacy.layout_revision = 1;
	legacy.tiles.pop_back(); // A player's older geometry must not be replaced by name.
	REQUIRE(legacy.IsValid());
	std::string old_export = legacy.ToJson();
	const std::string revision_field = "  \"layout_revision\": 1,\n";
	const auto revision_pos = old_export.find(revision_field);
	REQUIRE(revision_pos != std::string::npos);
	old_export.erase(revision_pos, revision_field.size());
	const auto path = fixture.root / "legacy-cst-v1.json";
	{ std::ofstream out(path); out << old_export; }
	std::string error;
	REQUIRE(BlueprintManager::ImportFromFile(path.string(), &error));
	legacy.is_builtin = false;
	CHECK(BlueprintManager::GetBlueprints().back().ToJson() == legacy.ToJson());
	CHECK(BlueprintManager::FindBuiltin(name)->ToJson() == current_layout);
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	CHECK(BlueprintManager::GetBlueprints().back().ToJson() == legacy.ToJson());
	CHECK(BlueprintManager::FindBuiltin(name)->ToJson() == current_layout);
	CHECK(FileBytes(path) == old_export);
	CHECK(legacy.Rotate(1).Mirror().layout_revision == 1);
}

TEST_CASE("Blueprint rename and delete retain actual backing filename", "[blueprint][storage]")
{
	PrivateBlueprintLibrary fixture;
	auto original = StorageBlueprint("Original Name");
	auto unusual = fixture.dir() / "not-derived-from-name.json";
	{ std::ofstream out(unusual); out << original.ToJson(); }
	std::string error;
	REQUIRE(BlueprintManager::RescanLibrary(&error));
	size_t index = BlueprintManager::GetBlueprints().size() - 1;
	REQUIRE(BlueprintManager::GetBlueprint(index)->name == "Original Name");
	REQUIRE(BlueprintManager::RenameBlueprint(index, "Renamed", &error));
	CHECK(std::filesystem::exists(unusual));
	auto reopened = Blueprint::FromJson(FileBytes(unusual));
	REQUIRE(reopened.has_value());
	CHECK(reopened->name == "Renamed");
	CHECK_FALSE(std::filesystem::exists(fixture.dir() / "Renamed.json"));
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	index = BlueprintManager::GetBlueprints().size() - 1;
	REQUIRE(BlueprintManager::GetBlueprint(index)->name == "Renamed");
	REQUIRE(BlueprintManager::DeleteBlueprint(index, &error));
	CHECK_FALSE(std::filesystem::exists(unusual));
}

TEST_CASE("Blueprint malformed files and name collisions leave library and bytes intact", "[blueprint][storage]")
{
	PrivateBlueprintLibrary fixture;
	std::string error;
	auto a = StorageBlueprint("A/B");
	auto b = StorageBlueprint("A?B");
	REQUIRE(BlueprintManager::SaveBlueprint(a, &error));
	REQUIRE(BlueprintManager::SaveBlueprint(b, &error));
	CHECK(std::filesystem::exists(fixture.dir() / "A_B.json"));
	CHECK(std::filesystem::exists(fixture.dir() / "A_B-1.json"));
	auto first_bytes = FileBytes(fixture.dir() / "A_B.json");
	{ std::ofstream out(fixture.dir() / "broken.json"); out << "not-json"; }
	CHECK_FALSE(BlueprintManager::RescanLibrary(&error));
	CHECK(error.find("broken.json") != std::string::npos);
	CHECK(BlueprintManager::GetBlueprints().size() == BlueprintManager::GetBuiltinCount() + 2);
	CHECK_FALSE(BlueprintManager::ImportFromFile((fixture.dir() / "broken.json").string(), &error));
	CHECK(FileBytes(fixture.dir() / "A_B.json") == first_bytes);
	size_t a_index = BlueprintManager::GetBuiltinCount();
	while (a_index < BlueprintManager::GetBlueprints().size() && BlueprintManager::GetBlueprint(a_index)->name != "A/B") ++a_index;
	REQUIRE(a_index < BlueprintManager::GetBlueprints().size());
	CHECK_FALSE(BlueprintManager::RenameBlueprint(a_index, "A?B", &error));
	CHECK(FileBytes(fixture.dir() / "A_B.json") == first_bytes);
}

TEST_CASE("Blueprint failed commit and missing backing file do not mutate memory", "[blueprint][storage]")
{
	PrivateBlueprintLibrary fixture;
	std::string error;
	auto bp = StorageBlueprint("Cannot Commit");
	REQUIRE(BlueprintManager::SaveBlueprint(bp, &error));
	size_t index = BlueprintManager::GetBlueprints().size() - 1;
	auto path = fixture.dir() / "Cannot Commit.json";
	auto old_bytes = FileBytes(path);
	std::filesystem::remove(path);
	std::filesystem::create_directory(path);
	CHECK_FALSE(BlueprintManager::RenameBlueprint(index, "New Name", &error));
	CHECK(BlueprintManager::GetBlueprint(index)->name == "Cannot Commit");
	CHECK_FALSE(BlueprintManager::DeleteBlueprint(index, &error));
	CHECK(BlueprintManager::GetBlueprint(index)->name == "Cannot Commit");
	CHECK_FALSE(BlueprintManager::SaveBlueprint(StorageBlueprint("Cannot Commit"), &error));
	CHECK(std::filesystem::is_directory(path));
	CHECK_FALSE(old_bytes.empty());
}

TEST_CASE("Blueprint symlinks and oversized files are skipped without changing real files", "[blueprint][storage]")
{
	PrivateBlueprintLibrary fixture;
	auto real = fixture.root / "external.json";
	auto bp = StorageBlueprint("External");
	{ std::ofstream out(real); out << bp.ToJson(); }
	auto link_path = fixture.dir() / "linked.json";
	std::filesystem::create_symlink(real, link_path);
	std::string error;
	CHECK_FALSE(BlueprintManager::RescanLibrary(&error));
	CHECK(error.find("linked.json") != std::string::npos);
	CHECK(BlueprintManager::GetBlueprints().size() == BlueprintManager::GetBuiltinCount());
	CHECK_FALSE(BlueprintManager::ImportFromFile(link_path.string(), &error));
	CHECK(FileBytes(real) == bp.ToJson());
	{ std::ofstream out(fixture.dir() / "large.json", std::ios::binary); out.seekp(Blueprint::MAX_JSON_BYTES); out.put('x'); }
	CHECK_FALSE(BlueprintManager::RescanLibrary(&error));
	CHECK(error.find("large.json") != std::string::npos);
	CHECK(BlueprintManager::GetBlueprints().size() == BlueprintManager::GetBuiltinCount());
}

TEST_CASE("Blueprint long names use bounded filenames and NUL paths are rejected", "[blueprint][storage]")
{
	PrivateBlueprintLibrary fixture;
	std::string error;
	auto bp = StorageBlueprint(std::string(220, 'N'));
	REQUIRE(BlueprintManager::SaveBlueprint(bp, &error));
	CHECK(std::filesystem::exists(fixture.dir() / (std::string(100, 'N') + ".json")));
	CHECK_FALSE(BlueprintManager::ExportToFile(bp, fixture.root.string() + std::string("/bad\0path", 9), &error));
	CHECK_FALSE(error.empty());
	CHECK_FALSE(BlueprintManager::ImportFromFile(std::string("bad\0path", 8), &error));
	CHECK_FALSE(error.empty());
}

#if defined(__linux__)
#include <dlfcn.h>
#include <cstdlib>
#endif

TEST_CASE("Blueprint storage injected IO failures preserve the prior file and memory", "[blueprint-io-worker]")
{
#if defined(__linux__)
	const char *operation = std::getenv("OST_BP_FAULT_OP");
	if (operation == nullptr) return; // Invoked by scripts/test_blueprint_io_failures.py.
	using Arm = void (*)(int);
	using Hits = int (*)();
	auto arm = reinterpret_cast<Arm>(dlsym(RTLD_DEFAULT, "BlueprintTestFaultArm"));
	auto hits = reinterpret_cast<Hits>(dlsym(RTLD_DEFAULT, "BlueprintTestFaultHits"));
	REQUIRE(arm != nullptr);
	REQUIRE(hits != nullptr);
	PrivateBlueprintLibrary fixture;
	std::string error;
	auto original = StorageBlueprint("Failure survivor");
	REQUIRE(BlueprintManager::SaveBlueprint(original, &error));
	const size_t index = BlueprintManager::GetBlueprints().size() - 1;
	const auto path = fixture.dir() / "Failure survivor.json";
	const auto bytes = FileBytes(path);
	const auto state = BlueprintManager::GetBlueprint(index)->ToJson();
	arm(1);
	bool result = std::string_view(operation) == "link" ?
		BlueprintManager::ExportToFile(original, (fixture.root / "publish.json").string(), &error) :
		BlueprintManager::RenameBlueprint(index, "Renamed survivor", &error);
	arm(0);
	REQUIRE(hits() == 1); // Prove the real file operation reached the requested failure point.
	CHECK_FALSE(result);
	CHECK_FALSE(error.empty());
	CHECK(FileBytes(path) == bytes);
	CHECK(BlueprintManager::GetBlueprint(index)->ToJson() == state);
	CHECK_FALSE(std::filesystem::exists(fixture.root / "publish.json"));
	for (const auto &entry : std::filesystem::directory_iterator(fixture.dir())) {
		CHECK(entry.path().filename().string().find(".tmp-") == std::string::npos);
	}
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	CHECK(BlueprintManager::GetBlueprints().back().ToJson() == state);
	REQUIRE(BlueprintManager::RenameBlueprint(BlueprintManager::GetBlueprints().size() - 1, "Successful retry", &error));
	CHECK(BlueprintManager::GetBlueprints().back().name == "Successful retry");
#else
	SUCCEED("Linux fault interposer runs through the dedicated script");
#endif
}
