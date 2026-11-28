#include "doctest.h"
#include <string>
#include <vector>
#include "Modules/Race/RaceHudLayout.h"

/*
 * The race HUD without the game: what each of its views says of a run.
 */

using namespace HudBlock;
using namespace PlayEvents;
using namespace RaceHudLayout;

namespace {

const unsigned long long kSecond = 1000;

std::string Text(const Row& row) {
	std::string text;
	for (const Cell& cell : row)
		text += (text.empty() ? "" : " ") + cell.text;
	return text;
}

std::vector<std::string> Texts(const Section& section) {
	std::vector<std::string> texts;
	for (const Row& row : section)
		texts.push_back(Text(row));
	return texts;
}

// A run of three splits, the first reached ten seconds ahead of the best run
// and the second twenty behind, with the third still to come.
Figures RunFigures() {
	RunModel::Run best = {};
	RunModel::Run run = {};
	run.name = "Act bosses (Normal)";
	const char* const names[] = { "Andariel", "Duriel", "Mephisto" };
	const unsigned long long bestAt[] = { 100, 250, 400 };
	const unsigned long long runAt[] = { 90, 270 };
	for (int i = 0; i < 3; i++) {
		best.splits.push_back({ names[i], RunModel::Split::Reached, false,
			bestAt[i] * kSecond, bestAt[i] * kSecond, 10 });
		run.splits.push_back({ names[i], RunModel::Split::Open, false, 0, 0, 0 });
		if (i < 2)
			run.splits[i] = { names[i], RunModel::Split::Reached, false,
				runAt[i] * kSecond, runAt[i] * kSecond, 12 };
	}
	best.ended = best.finished = true;
	best.realMs = best.gameMs = 400 * kSecond;

	RunStore store;
	store.Record("key", best);
	return FiguresOfRun(run, 300 * kSecond, store.Find("key"), RunStore::RealTime);
}

// The rows of the splits view that are the run's splits and its clock: the
// section under the route's name.
Section SplitsOf(const Figures& figures) {
	const std::vector<Section> sections = Sections(ViewSplits, figures);
	REQUIRE(sections.size() == 2);
	return sections[1];
}

}

TEST_CASE("the views are named, and a name finds its view whatever its case") {
	CHECK(std::string(NameOf(ViewLine)) == "Line");
	CHECK(std::string(NameOf(ViewSplits)) == "Splits");
	CHECK(ViewNamed("line") == ViewLine);
	CHECK(ViewNamed("SPLITS") == ViewSplits);
	CHECK(ViewNamed("compact") == ViewCount);
	CHECK(ViewNamed("") == ViewCount);
}

TEST_CASE("stepping through the views goes round, either way") {
	CHECK(Next(ViewLine, 1) == ViewSplits);
	CHECK(Next(ViewSplits, 1) == ViewLine);
	CHECK(Next(ViewLine, -1) == ViewSplits);
	CHECK(Next(ViewSplits, -3) == ViewLine);
}

TEST_CASE("with no run to show there is nothing to say, in any view") {
	Figures none;
	none.inRun = false;
	CHECK(Sections(ViewLine, none).empty());
	CHECK(Sections(ViewSplits, none).empty());
}

TEST_CASE("the line view is the split the run is on, its clock and how it stands") {
	const std::vector<Section> sections = Sections(ViewLine, RunFigures());
	REQUIRE(sections.size() == 1);
	CHECK(Text(sections[0][0]) == "Mephisto 5:00 +20.0");
	CHECK(sections[0][0][2].tone == ToneBehind);
}

TEST_CASE("the line view has no standing before a split has been compared") {
	Figures figures = RunFigures();
	for (RunCompare::Line& line : figures.run.splits)
		line.compared = false;
	CHECK(Text(Sections(ViewLine, figures)[0][0]) == "Mephisto 5:00");
}

TEST_CASE("the splits view is the route's name over its splits") {
	const std::vector<Section> sections = Sections(ViewSplits, RunFigures());
	REQUIRE(sections.size() == 2);
	CHECK(Texts(sections[0]) == std::vector<std::string>{ "Act bosses (Normal)" });
	CHECK(sections[0][0][0].tone == ToneLabel);
}

TEST_CASE("the splits view lists the route, with the time to beat for what is still to come") {
	const Section splits = SplitsOf(RunFigures());
	// The run's clock and the best run's stand under the times, in their column.
	CHECK(Texts(splits) == std::vector<std::string>{
		"Andariel 1:30 -10.0", "Duriel 4:30 +20.0", "Mephisto 6:40 ", "Time 5:00 ", "Best 6:40 " });
	CHECK(splits[3][1].tone == ToneValue);
	CHECK(splits[4][1].tone == ToneDim);
}

TEST_CASE("a run with no best to beat has a clock and nothing under it") {
	Figures figures = RunFigures();
	figures.run.bestTotal = -1;
	CHECK(Texts(SplitsOf(figures)).back() == "Time 5:00 ");
}

TEST_CASE("a split's standing is ahead, behind, or the best that stretch has been done") {
	Figures figures = RunFigures();
	Section splits = SplitsOf(figures);
	// The first stretch took 90 where the best was 100.
	CHECK(splits[0][2].tone == ToneBest);
	CHECK(splits[1][2].tone == ToneBehind);

	figures.run.splits[0].gold = false;
	splits = SplitsOf(figures);
	CHECK(splits[0][2].tone == ToneAhead);
}

TEST_CASE("the split a run is on is written as a figure, where it stood as a name") {
	const Section splits = SplitsOf(RunFigures());
	CHECK(splits[0][0].tone == ToneLabel);
	CHECK(splits[2][0].tone == ToneValue);
	CHECK(splits[2][0].left);
	// A time reached is the run's own, and the time to beat is not.
	CHECK(splits[1][1].tone == ToneValue);
	CHECK(splits[2][1].tone == ToneDim);
}

TEST_CASE("a long route shows the splits around the one the run is on") {
	Figures figures = RunFigures();
	figures.run.splits.clear();
	for (int i = 0; i < 15; i++) {
		RunCompare::Line line = {};
		line.name = "Split " + std::to_string(i + 1);
		line.state = i < 9 ? RunModel::Split::Reached : RunModel::Split::Open;
		line.time = i < 9 ? (i + 1) * 60 * (long long)kSecond : -1;
		line.best = -1;
		figures.run.splits.push_back(line);
	}
	figures.run.current = 9;

	// Eight splits, and the run's clock and the best run's under them.
	const Section splits = SplitsOf(figures);
	REQUIRE(splits.size() == 10);
	CHECK(splits.front()[0].text == "Split 7");
	CHECK(splits[3][0].text == "Split 10");
	CHECK(splits[7][0].text == "Split 14");

	// Near the end it is the last eight, so the block does not shrink.
	figures.run.current = 14;
	CHECK(SplitsOf(figures).front()[0].text == "Split 8");
	CHECK(SplitsOf(figures)[7][0].text == "Split 15");
}

TEST_CASE("a run that has finished says so where it said which split it was on") {
	Figures figures = RunFigures();
	figures.runEnded = true;
	figures.run.current = -1;
	CHECK(Text(Sections(ViewLine, figures)[0][0]).substr(0, 8) == "Finished");
}

TEST_CASE("a route not yet run is its splits with the times to beat, and a clock at nothing") {
	const RunModel::Run run = RunModel::Unstarted("act1", "Act 1", { "Andariel", "Duriel" });
	const Figures figures = FiguresOfRun(run, 0, NULL, RunStore::RealTime);
	CHECK(figures.inRun);
	CHECK_FALSE(figures.runEnded);
	CHECK(figures.route == "Act 1");
	CHECK(Texts(SplitsOf(figures)) == std::vector<std::string>{
		"Andariel  ", "Duriel  ", "Time 0:00 " });
}

TEST_CASE("a run in hand is read off its events, on the clock asked for") {
	RunModel model;
	model.Apply({ 0, Joined{ "a", 0, 1, 0 } });
	model.Apply({ 10 * kSecond, RunStarted{ "bosses-normal", "Act bosses (Normal)", { "Andariel", "Duriel" } } });
	model.Apply({ 60 * kSecond, Left() });
	model.Apply({ 90 * kSecond, Joined{ "b", 0, 5, 0 } });
	model.Apply({ 100 * kSecond, SplitReached{ 0, false } });

	const Figures real = FiguresOf(model, 130 * kSecond, NULL, RunStore::RealTime);
	CHECK(real.inRun);
	CHECK_FALSE(real.runEnded);
	CHECK(real.route == "Act bosses (Normal)");
	CHECK(real.run.clock == 120 * (long long)kSecond);
	CHECK(real.run.splits[0].time == 90 * (long long)kSecond);
	CHECK(real.run.current == 1);

	// Thirty seconds of it were spent between two games.
	const Figures game = FiguresOf(model, 130 * kSecond, NULL, RunStore::GameTime);
	CHECK(game.run.clock == 90 * (long long)kSecond);
	CHECK(game.run.splits[0].time == 60 * (long long)kSecond);
}

TEST_CASE("a character never timed has no run to show") {
	RunModel model;
	model.Apply({ 0, Joined{ "a", 0, 1, 0 } });
	CHECK_FALSE(FiguresOf(model, 10 * kSecond).inRun);
}

TEST_CASE("a finished run stays to be read for a minute, and one given up does not") {
	RunModel model;
	model.Apply({ 0, Joined{ "a", 0, 1, 0 } });
	model.Apply({ 0, RunStarted{ "act1", "Act 1 (Andariel)", { "Andariel" } } });
	model.Apply({ 100 * kSecond, SplitReached{ 0, false } });
	model.Apply({ 100 * kSecond, RunEnded{ true } });

	const Figures soon = FiguresOf(model, 130 * kSecond);
	CHECK(soon.inRun);
	CHECK(soon.runEnded);
	CHECK(soon.run.clock == 100 * (long long)kSecond);
	CHECK_FALSE(FiguresOf(model, 161 * kSecond).inRun);

	model.Apply({ 200 * kSecond, RunStarted{ "act1", "Act 1 (Andariel)", { "Andariel" } } });
	model.Apply({ 210 * kSecond, RunEnded{ false } });
	CHECK_FALSE(FiguresOf(model, 211 * kSecond).inRun);
}
