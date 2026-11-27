#include "doctest.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <string>
#include "Modules/Forge/ForgeFile.h"
#include "Modules/Forge/ForgeStore.h"

/*
 * The Forge's file: how a marker is read from it, and what a save writes back.
 */

namespace {

MapIconStyle Defaults() {
	MapIconStyle style;
	style.icon = MapIcon::Square;
	style.size = 1;
	style.fillColor = 0xFF;
	style.borderColor = MapIcon::NoColor;
	style.borderWidth = 1;
	style.line = false;
	style.lineColor = 0xFF;
	return style;
}

std::string Saved(const ForgeStore& store, const std::string& onDisk = "") {
	std::string text;
	REQUIRE(store.SaveOnto(onDisk, text));
	return text;
}

const std::vector<std::string> Scopes = { "lower kurast", "every area" };

}  // namespace

TEST_CASE("A marker the file does not have is its defaults, and nothing to save") {
	ForgeStore store;
	REQUIRE(store.Load(""));
	MapIconStyle style;
	style.size = 7;
	store.BindStyle("chests", "locked", &style, Defaults());
	CHECK(style == Defaults());
	CHECK_FALSE(store.HasChanges());
}

TEST_CASE("A marker is read from its tab, field by field") {
	ForgeStore store;
	REQUIRE(store.Load(R"({ "version": 1, "chests": { "locked": {
		"icon": "Diamond", "size": 3, "fill": "0x97", "border": "none",
		"borderWidth": 2, "line": true, "lineColor": "105" } } })"));
	MapIconStyle style;
	store.BindStyle("chests", "locked", &style, Defaults());
	CHECK(style.icon == MapIcon::Diamond);
	CHECK(style.size == 3);
	CHECK(style.fillColor == 0x97);
	CHECK(style.borderColor == MapIcon::NoColor);
	CHECK(style.borderWidth == 2);
	CHECK(style.line);
	CHECK(style.lineColor == 105);
	CHECK_FALSE(store.HasChanges());
}

TEST_CASE("Black and no colour are different values") {
	ForgeStore store;
	REQUIRE(store.Load(R"({ "chests": { "chest": { "fill": "0x00", "border": "none" },
		"locked": { "fill": 0, "border": "0x00" } } })"));
	MapIconStyle chest, locked;
	store.BindStyle("chests", "chest", &chest, Defaults());
	store.BindStyle("chests", "locked", &locked, Defaults());
	CHECK(chest.fillColor == 0x00);
	CHECK(chest.borderColor == MapIcon::NoColor);
	CHECK(locked.fillColor == 0x00);
	CHECK(locked.borderColor == 0x00);
}

TEST_CASE("A value that cannot be used is the default, and one out of range is clamped") {
	ForgeStore store;
	REQUIRE(store.Load(R"({ "chests": { "chest": {
		"icon": "hexagon", "size": 99, "fill": "0x100", "border": "red",
		"borderWidth": -3, "line": "yes", "lineColor": "none" } } })"));
	MapIconStyle style;
	MapIconStyle defaults = Defaults();
	defaults.lineColor = 0x20;
	store.BindStyle("chests", "chest", &style, defaults);
	CHECK(style.icon == MapIcon::Square);
	CHECK(style.size == MapIcon::MaxSize);
	CHECK(style.fillColor == 0xFF);
	CHECK(style.borderColor == MapIcon::NoColor);
	CHECK(style.borderWidth == MapIcon::MinBorder);
	CHECK_FALSE(style.line);
	// The line always has a colour.
	CHECK(style.lineColor == 0x20);
}

TEST_CASE("A changed marker is written whole, and an untouched one not at all") {
	ForgeStore store;
	REQUIRE(store.Load(""));
	MapIconStyle chest, locked;
	unsigned int scope = 0;
	store.BindStyle("chests", "chest", &chest, Defaults());
	store.BindStyle("chests", "locked", &locked, Defaults());
	store.BindNamed("chests", "locked", "scope", &scope, Scopes, 0);

	locked.icon = MapIcon::Star;
	locked.fillColor = 0x00;
	CHECK(store.HasChanges());
	CHECK(Saved(store) ==
		"{\n"
		"  \"version\": 1,\n"
		"  \"chests\": {\n"
		"    \"locked\": {\n"
		"      \"icon\": \"star\",\n"
		"      \"size\": 1,\n"
		"      \"fill\": \"0x00\",\n"
		"      \"border\": \"none\",\n"
		"      \"borderWidth\": 1,\n"
		"      \"line\": false,\n"
		"      \"lineColor\": \"0xFF\",\n"
		"      \"scope\": \"lower kurast\"\n"
		"    }\n"
		"  }\n"
		"}\n");
}

TEST_CASE("What was read and is saved again reads back the same") {
	ForgeStore first;
	REQUIRE(first.Load(""));
	MapIconStyle style;
	unsigned int scope = 0;
	first.BindStyle("chests", "super", &style, Defaults());
	first.BindNamed("chests", "super", "scope", &scope, Scopes, 0);
	style.icon = MapIcon::Chest;
	style.size = 5;
	style.borderColor = 0x0A;
	style.borderWidth = 3;
	style.line = true;
	style.lineColor = 0x69;
	scope = 1;
	std::string text = Saved(first);

	ForgeStore second;
	REQUIRE(second.Load(text));
	MapIconStyle read;
	unsigned int readScope = 0;
	second.BindStyle("chests", "super", &read, Defaults());
	second.BindNamed("chests", "super", "scope", &readScope, Scopes, 0);
	CHECK(read == style);
	CHECK(readScope == 1);
}

TEST_CASE("A marker put back to its defaults leaves the file, and its tab with it") {
	ForgeStore store;
	std::string text = R"({ "version": 1, "chests": { "locked": { "icon": "star" } } })";
	REQUIRE(store.Load(text));
	MapIconStyle style;
	store.BindStyle("chests", "locked", &style, Defaults());
	REQUIRE(style.icon == MapIcon::Star);

	style.icon = MapIcon::Square;
	CHECK(Saved(store, text) == "{\n  \"version\": 1\n}\n");
}

TEST_CASE("What nothing has bound is kept as it was") {
	std::string text = R"({ "version": 1,
		"shrines": { "experience": { "icon": "star" } },
		"chests": { "locked": { "icon": "star", "glow": true }, "chest": { "size": 2 } } })";
	ForgeStore store;
	REQUIRE(store.Load(text));
	MapIconStyle locked;
	store.BindStyle("chests", "locked", &locked, Defaults());
	locked.icon = MapIcon::Square;

	ForgeStore reread;
	REQUIRE(reread.Load(Saved(store, text)));
	MapIconStyle chest, shrine, back;
	reread.BindStyle("chests", "chest", &chest, Defaults());
	reread.BindStyle("shrines", "experience", &shrine, Defaults());
	reread.BindStyle("chests", "locked", &back, Defaults());
	CHECK(chest.size == 2);
	CHECK(shrine.icon == MapIcon::Star);
	CHECK(back == Defaults());
	// The field this version does not know keeps its marker in the file.
	CHECK(Saved(store, text).find("\"glow\": true") != std::string::npos);
}

TEST_CASE("A save is made onto what another client saved since") {
	ForgeStore mine;
	REQUIRE(mine.Load(""));
	MapIconStyle chest, locked;
	mine.BindStyle("chests", "chest", &chest, Defaults());
	mine.BindStyle("chests", "locked", &locked, Defaults());
	locked.size = 4;

	std::string theirs = R"({ "version": 1, "chests": { "chest": { "icon": "circle" } } })";
	ForgeStore reread;
	REQUIRE(reread.Load(Saved(mine, theirs)));
	MapIconStyle readChest, readLocked;
	reread.BindStyle("chests", "chest", &readChest, Defaults());
	reread.BindStyle("chests", "locked", &readLocked, Defaults());
	CHECK(readChest.icon == MapIcon::Circle);
	CHECK(readLocked.size == 4);
}

TEST_CASE("Text that is not the file's shape says where, and is not saved over") {
	ForgeStore store;
	std::string error;
	CHECK_FALSE(store.Load("{\n  \"chests\": {\n    \"chest\" 1\n  }\n}", &error));
	CHECK(error == "line 3, column 13");

	CHECK_FALSE(store.Load("  [1, 2]", &error));
	CHECK(error == "line 1, column 3");

	MapIconStyle style;
	store.BindStyle("chests", "chest", &style, Defaults());
	style.size = 3;
	std::string text;
	CHECK_FALSE(store.SaveOnto("{ \"chests\": ", text, &error));
	CHECK(text.empty());
}

TEST_CASE("Comments are refused rather than lost on the next save") {
	ForgeStore store;
	CHECK_FALSE(store.Load("{ // mine\n}"));
}

TEST_CASE("A later version's number is kept") {
	ForgeStore store;
	std::string text = R"({ "chests": {}, "version": 4 })";
	REQUIRE(store.Load(text));
	MapIconStyle style;
	store.BindStyle("chests", "chest", &style, Defaults());
	style.size = 2;
	CHECK(Saved(store, text).find("{\n  \"version\": 4,") == 0);
}

TEST_CASE("Saved values are no longer changes, and a reload binds afresh") {
	ForgeStore store;
	REQUIRE(store.Load(""));
	MapIconStyle style;
	store.BindStyle("chests", "chest", &style, Defaults());
	style.size = 6;
	std::string text = Saved(store);
	store.MarkSaved(text);
	CHECK_FALSE(store.HasChanges());

	REQUIRE(store.Load(text));
	style = Defaults();
	store.BindStyle("chests", "chest", &style, Defaults());
	store.BindStyle("chests", "chest", &style, Defaults());
	CHECK(style.size == 6);
	CHECK_FALSE(store.HasChanges());
	style.size = 1;
	CHECK(Saved(store, text) == "{\n  \"version\": 1\n}\n");
}

namespace {

class TempPath {
	private:
		std::string path;

	public:
		explicit TempPath(const char* named) {
			char directory[MAX_PATH];
			DWORD length = GetTempPathA(MAX_PATH, directory);
			REQUIRE(length > 0);
			std::stringstream built;
			built << std::string(directory, length) << "BH_" << named << "_"
				<< GetCurrentProcessId() << ".json";
			path = built.str();
			DeleteFileA(path.c_str());
		}

		~TempPath() {
			DeleteFileA(path.c_str());
			DeleteFileA((path + ".tmp").c_str());
		}

		const std::string& Path() const { return path; }

		void Write(const std::string& text) const {
			std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
			REQUIRE(file.is_open());
			file << text;
		}

		std::string Read() const {
			std::ifstream file(path.c_str(), std::ios::binary);
			if (!file.is_open())
				return std::string();
			std::stringstream held;
			held << file.rdbuf();
			return held.str();
		}

		bool Exists() const {
			return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
		}
};

}  // namespace

TEST_CASE("A Forge file is only created once something differs from its defaults") {
	TempPath temp("forge_create");
	ForgeFile file(temp.Path());
	REQUIRE(file.Load());
	MapIconStyle style;
	file.GetStore().BindStyle("chests", "chest", &style, Defaults());
	CHECK(file.Save());
	CHECK_FALSE(temp.Exists());

	style.size = 2;
	CHECK(file.Save());
	CHECK(temp.Read().find("\"size\": 2") != std::string::npos);
	CHECK(file.GetProblem().empty());
}

TEST_CASE("A Forge file that is not valid is left as it is, and says why") {
	TempPath temp("forge_invalid");
	std::string broken = "{ \"chests\": { \"chest\": { \"size\": 2, } } }";
	temp.Write(broken);
	ForgeFile file(temp.Path());
	CHECK_FALSE(file.Load());
	CHECK(file.GetProblem().find("Error in BH_forge_invalid_") == 0);
	CHECK(file.GetProblem().find(".json at line 1, column 37") != std::string::npos);

	MapIconStyle style;
	file.GetStore().BindStyle("chests", "chest", &style, Defaults());
	CHECK(style == Defaults());
	style.size = 3;
	CHECK_FALSE(file.Save());
	CHECK(temp.Read() == broken);

	// Fixed by hand, the next save goes onto what the player fixed.
	temp.Write("{ \"chests\": { \"locked\": { \"size\": 4 } } }");
	CHECK(file.Save());
	CHECK(file.GetProblem().empty());
	CHECK(temp.Read().find("\"locked\"") != std::string::npos);
	CHECK(temp.Read().find("\"size\": 3") != std::string::npos);
}

namespace {

// A list of numbers kept as a tab's own value, the way a module keeps one.
struct Numbers {
	std::string text = "[]";
	bool given = false;

	void Bind(ForgeStore& store) {
		store.BindValue("monsters", "rules",
			[this]() { return text; },
			[this](const std::string* stored) {
				given = stored != NULL;
				text = stored ? *stored : "[]";
			},
			[this]() { return text == "[]"; });
	}
};

}  // namespace

TEST_CASE("A tab's own value is read from beside its markers, and is its default without one") {
	ForgeStore store;
	REQUIRE(store.Load(R"({ "monsters": { "boss": { "size": 5 }, "rules": [1, 2] } })"));
	Numbers numbers;
	numbers.Bind(store);
	CHECK(numbers.given);
	CHECK(numbers.text == "[1,2]");
	CHECK_FALSE(store.HasChanges());

	ForgeStore empty;
	REQUIRE(empty.Load(""));
	Numbers none;
	none.text = "[9]";
	none.Bind(empty);
	CHECK_FALSE(none.given);
	CHECK(none.text == "[]");
	CHECK_FALSE(empty.HasChanges());
}

TEST_CASE("A tab's own value is written beside its markers, and leaves the file at its default") {
	ForgeStore store;
	REQUIRE(store.Load(""));
	MapIconStyle style;
	store.BindStyle("monsters", "boss", &style, Defaults());
	Numbers numbers;
	numbers.Bind(store);

	numbers.text = "[3, {\"minLevel\": 85}]";
	style.size = 5;
	CHECK(store.HasChanges());
	std::string text = Saved(store);
	CHECK(text.find("\"rules\": [\n      3,") != std::string::npos);
	CHECK(text.find("\"minLevel\": 85") != std::string::npos);
	CHECK(text.find("\"boss\": {") != std::string::npos);
	store.MarkSaved(text);
	CHECK_FALSE(store.HasChanges());

	// Back at its default it goes, and the marker beside it stays.
	numbers.text = "[]";
	CHECK(store.HasChanges());
	text = Saved(store, text);
	CHECK(text.find("rules") == std::string::npos);
	CHECK(text.find("\"boss\": {") != std::string::npos);

	store.MarkSaved(text);
	style = Defaults();
	text = Saved(store, text);
	CHECK(text.find("monsters") == std::string::npos);
}

TEST_CASE("A tab's own value this client left alone is another client's to change") {
	ForgeStore store;
	REQUIRE(store.Load(R"({ "monsters": { "rules": [1] } })"));
	MapIconStyle style;
	store.BindStyle("monsters", "boss", &style, Defaults());
	Numbers numbers;
	numbers.Bind(store);

	style.size = 6;
	std::string text = Saved(store, R"({ "monsters": { "rules": [1, 2, 3] } })");
	CHECK(text.find("3") != std::string::npos);
	CHECK(text.find("\"size\": 6") != std::string::npos);
}
