#pragma once
#include <string>
#include <vector>
#include "SplitRoute.h"

// Changing routes: the splits there are to choose from, and what can be done to
// a route and to the list of them. Nothing here knows how it is asked for.
namespace RouteEdit {

// The splits a route can be built from without writing one out: every act
// boss, quest and quest item, in each difficulty, with what marks it reached.
const std::vector<Splits::Split>& Catalogue();

// A split for reaching a level, from text that asks for one: "level 24".
bool LevelSplit(const std::string& text, Splits::Split& split);

// What marks a split reached, in a few words: "Normal: kill or quest".
std::string Describe(const Splits::Split& split);

// Adds a route, a copy of the one given or with no splits where there is none,
// under a name and an id no other route has. Answers its id.
std::string Add(std::vector<Splits::Route>& routes, const Splits::Route* copyOf);

void Remove(std::vector<Splits::Route>& routes, const std::string& id);
void Rename(std::vector<Splits::Route>& routes, const std::string& id, const std::string& name);

// Puts back the routes BH comes with that are missing, by their ids, and leaves
// the rest as they are.
void Restore(std::vector<Splits::Route>& routes);

// Adds a split after the one at `after`, or at the end where that is no split
// of the route. Answers where it went.
unsigned int AddSplit(Splits::Route& route, const Splits::Split& split, int after);
void RemoveSplit(Splits::Route& route, unsigned int index);

// Moves a split one place towards the start (-1) or the end (1). False where
// there is nowhere for it to go.
bool MoveSplit(Splits::Route& route, unsigned int index, int by);

}
