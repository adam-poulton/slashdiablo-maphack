#include "ModuleManager.h"
#include "../About.h"
#include "Settings/SettingsRegistry.h"
#include "Module.h"
#include "../D2Helpers.h"
#include "../BH.h"
#include <algorithm>
#include <iterator>

// The commands BH answers itself rather than through a module. Answering and
// printing both read this, so a name BH takes and a name BH advertises cannot
// drift apart.
static std::vector<ChatCommand> OwnCommands() {
	std::vector<ChatCommand> commands;
	commands.push_back(ChatCommand{ "help", { "commands" }, "",
		"Lists every command BH answers" });
	commands.push_back(ChatCommand{ "reload", {}, "",
		"Rereads BH.cfg and BH_settings.cfg from disk" });
	commands.push_back(ChatCommand{ "save", {}, "",
		"Writes the current settings back to BH_settings.cfg" });
	commands.push_back(ChatCommand{ "version", {}, "",
		"Prints which build of BH and of the game this is" });
	return commands;
}

// The commands the game answers for itself. BH sees every command first and has
// no way to ask the game whether it knows one, so without this a command that
// worked perfectly well would be answered with a hint about BH's. These are what
// the game's own help advertises; a realm that adds more belongs here too.
static const char* kGameCommands[] = { "claim" };

ModuleManager::ModuleManager() {

}

ModuleManager::~ModuleManager() {
	for (auto it = moduleList.begin(); it != moduleList.end(); ++it) {
		Module* module = (*it).second;
		delete module;
	}
	moduleList.clear();
}

void ModuleManager::FixName(std::string& name)
{
	std::transform(name.begin(), name.end(), name.begin(), tolower);
	std::replace(name.begin(), name.end(), ' ', '-');
}

void ModuleManager::Add(Module* module) {
	// Add to list of modules
	std::string name = module->GetName();
	FixName(name);
	moduleList[name] = module;
}

Module* ModuleManager::Get(string name) {
	// Through the same folding Add() used, so that a module can be looked up by
	// the name it calls itself. Without this, Get() only worked for names that
	// happened to already be lowercase and unspaced.
	FixName(name);
	if (moduleList.count(name) > 0) {
		return moduleList[name];
	}
	return NULL;
}

void ModuleManager::Remove(Module* module) {
	// Remove module from list
	std::string name = module->GetName();
	FixName(name);
	moduleList.erase(name);

	delete module;
}

void ModuleManager::LoadModules() {
	for (map<string, Module*>::iterator it = moduleList.begin(); it != moduleList.end(); ++it) {
		(*it).second->Load();
	}
}

void ModuleManager::UnloadModules() {
	for (map<string, Module*>::iterator it = moduleList.begin(); it != moduleList.end(); ++it) {
		(*it).second->Unload();
	}
}

void ModuleManager::ReloadConfig() {
	for (map<string, Module*>::iterator it = moduleList.begin(); it != moduleList.end(); ++it) {
		(*it).second->LoadConfig();
	}
}

void ModuleManager::MpqLoaded() {
	for (map<string, Module*>::iterator it = moduleList.begin(); it != moduleList.end(); ++it) {
		(*it).second->MpqLoaded();
	}
}

// ".cube (.recipe, .recipes) <search>": every name that reaches the command, and
// then what it takes after one of them.
static std::string CommandText(const ChatCommand& command) {
	std::string text = "." + command.name;
	if (!command.aliases.empty()) {
		text += " (";
		for (unsigned int i = 0; i < command.aliases.size(); i++) {
			if (i > 0)
				text += ", ";
			text += "." + command.aliases[i];
		}
		text += ")";
	}
	if (command.args.length() > 0)
		text += " " + command.args;
	return text;
}

// A line per command under a line naming what owns them, indented under it.
// Nothing is lined up into columns: the chat log is the game's to draw in a font
// of its own, so padding with spaces would not line up anyway.
static void PrintGroup(const std::string& label,
		const std::vector<ChatCommand>& commands) {
	Print("\377c4%s:", label.c_str());
	for (unsigned int i = 0; i < commands.size(); i++) {
		std::string text = CommandText(commands[i]);
		if (commands[i].description.length() > 0) {
			Print("  \377c4%s\377c0 - %s", text.c_str(),
				commands[i].description.c_str());
		} else {
			Print("  \377c4%s", text.c_str());
		}
	}
}

void ModuleManager::PrintCommands() {
	PrintGroup("BH", OwnCommands());

	for (map<string, Module*>::iterator it = moduleList.begin();
			it != moduleList.end(); ++it) {
		std::vector<ChatCommand> commands = it->second->GetCommands();
		if (!commands.empty())
			PrintGroup(it->second->GetName(), commands);
	}
}

void ModuleManager::HintCommands(const std::string& command) {
	for (int i = 0; i < (int)(sizeof(kGameCommands) / sizeof(kGameCommands[0])); i++) {
		if (command.compare(kGameCommands[i]) == 0)
			return;
	}
	Print("\377c4BH:\377c0 no such BH command. Type \377c3.help\377c0 for the list.");
}

bool ModuleManager::UserInput(wchar_t* module, wchar_t* msg, bool fromGame) {
	bool block = false;
	std::string name;
	std::wstring modname(module);
	name = WStringToString(modname);
	transform(name.begin(), name.end(), name.begin(), ::tolower);

	// Whichever of BH's own commands was reached, under the one name the rest of
	// this knows it by, so an alias is said in the list and nowhere else.
	std::vector<ChatCommand> own = OwnCommands();
	std::string command;
	for (unsigned int i = 0; i < own.size() && command.empty(); i++) {
		if (own[i].Answers(name))
			command = own[i].name;
	}

	if (command.compare("reload") == 0)
	{
		ReloadConfig();
		Print("\377c4BH:\377c0 Successfully reloaded configuration.");
		return true;
	}

	if (command.compare("version") == 0) {
		// The same lines the menus and the settings window show, from the same
		// place, so what gets reported cannot differ from what is on screen.
		std::vector<std::string> lines = About::Lines();
		for (unsigned int i = 0; i < lines.size(); i++)
			Print("\377c4BH:\377c0 %s", lines[i].c_str());
		return true;
	}

	if (command.compare("save") == 0) {
		Settings::Persist();
		Print("\377c4BH:\377c0 Successfully saved configuration.");
		return true;
	}

	if (command.compare("help") == 0) {
		PrintCommands();
		return true;
	}

	// A module is reached by its own name, or by any of the shorter commands it
	// claims for itself.
	Module* target = NULL;
	map<string, Module*>::iterator named = moduleList.find(name);
	if (named != moduleList.end()) {
		target = named->second;
	} else {
		for (map<string, Module*>::iterator it = moduleList.begin();
				it != moduleList.end() && !target; ++it) {
			if (it->second->OwnsCommand(name))
				target = it->second;
		}
	}

	if (target) {
		// Which command was typed is part of what the module is being asked, so
		// leave it where the handler can read it.
		target->invokedCommand = name;
		__raise target->UserInput(msg, fromGame, &block);
	} else {
		// Nothing is blocked either way, so the game still gets its turn at a
		// command that turns out to be one of its own.
		HintCommands(name);
	}
	return block;
}
