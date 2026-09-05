#include "AccountStore.h"
#include "../../StringUtil.h"
#include <algorithm>
#include "nlohmann/json.hpp"

namespace {

// Names are told apart without regard to case, as the realm tells them apart,
// so that one account cannot be kept twice under two spellings of itself.
bool SameName(const std::string& left, const std::string& right) {
	return ToLower(left).compare(ToLower(right)) == 0;
}

// The order accounts and rosters are drawn in. Alphabetical without regard to
// case, so that a list a player has learned the shape of keeps that shape:
// nothing an account is used for moves it.
bool BeforeByName(const Account& left, const Account& right) {
	return ToLower(left.name).compare(ToLower(right.name)) < 0;
}

bool BeforeAlphabetically(const std::string& left, const std::string& right) {
	return ToLower(left).compare(ToLower(right)) < 0;
}

// What the file says of itself. Read but not insisted on, since the only shape
// there has ever been is this one; written so that a reader of a later shape has
// something to tell them apart by.
const int kFileVersion = 1;

// What one member of an account says, or nothing, without regard to what a
// player has managed to put there. Asked by type rather than with value(),
// which throws when the type is not the one the default is: a corrupted file
// has to come back as an answer and never as a throw out of a drawing path.
std::string StringAt(const nlohmann::json& object, const char* key) {
	nlohmann::json::const_iterator found = object.find(key);
	if (found == object.end() || !found->is_string())
		return std::string();
	return found->get<std::string>();
}

bool BoolAt(const nlohmann::json& object, const char* key) {
	nlohmann::json::const_iterator found = object.find(key);
	if (found == object.end() || !found->is_boolean())
		return false;
	return found->get<bool>();
}

}  // namespace

Account* AccountStore::Lookup(const std::string& name) {
	for (unsigned int i = 0; i < accounts.size(); i++) {
		if (SameName(accounts[i].name, name))
			return &accounts[i];
	}
	return NULL;
}

bool AccountStore::Save(const std::string& name, const std::string& password) {
	std::string trimmedName = Trim(name);
	std::string trimmedPassword = Trim(password);
	if (trimmedName.empty() || trimmedPassword.empty())
		return false;

	Account* kept = Lookup(trimmedName);
	if (kept) {
		kept->password = trimmedPassword;
		return true;
	}

	Account account;
	account.name = trimmedName;
	account.password = trimmedPassword;
	accounts.push_back(account);
	return true;
}

bool AccountStore::Forget(const std::string& name) {
	for (std::vector<Account>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
		if (SameName(it->name, name)) {
			accounts.erase(it);
			return true;
		}
	}
	return false;
}

const Account* AccountStore::Find(const std::string& name) const {
	for (unsigned int i = 0; i < accounts.size(); i++) {
		if (SameName(accounts[i].name, name))
			return &accounts[i];
	}
	return NULL;
}

bool AccountStore::SetRoster(const std::string& name, const std::string& roster) {
	Account* kept = Lookup(name);
	if (!kept)
		return false;
	kept->roster = Trim(roster);
	return true;
}

bool AccountStore::SetFavourite(const std::string& name, bool favourite) {
	Account* kept = Lookup(name);
	if (!kept)
		return false;
	kept->favourite = favourite;
	return true;
}

std::vector<Account> AccountStore::Favourites() const {
	std::vector<Account> found;
	for (unsigned int i = 0; i < accounts.size(); i++) {
		if (accounts[i].favourite)
			found.push_back(accounts[i]);
	}
	std::sort(found.begin(), found.end(), BeforeByName);
	return found;
}

std::vector<std::string> AccountStore::Rosters() const {
	std::vector<std::string> found;
	for (unsigned int i = 0; i < accounts.size(); i++) {
		const std::string& roster = accounts[i].roster;
		// A favourite is drawn above the rosters, so it does not name one for
		// itself: a roster nothing else is kept in would head an empty list.
		if (roster.empty() || accounts[i].favourite)
			continue;
		bool named = false;
		for (unsigned int j = 0; j < found.size(); j++) {
			if (ToLower(found[j]).compare(ToLower(roster)) == 0) {
				named = true;
				break;
			}
		}
		if (!named)
			found.push_back(roster);
	}
	std::sort(found.begin(), found.end(), BeforeAlphabetically);
	return found;
}

std::vector<Account> AccountStore::InRoster(const std::string& roster) const {
	std::vector<Account> found;
	for (unsigned int i = 0; i < accounts.size(); i++) {
		if (accounts[i].favourite)
			continue;
		if (ToLower(accounts[i].roster).compare(ToLower(Trim(roster))) == 0)
			found.push_back(accounts[i]);
	}
	std::sort(found.begin(), found.end(), BeforeByName);
	return found;
}

std::string AccountStore::ToJson() const {
	nlohmann::json listed = nlohmann::json::array();
	for (unsigned int i = 0; i < accounts.size(); i++) {
		nlohmann::json account;
		account["name"] = accounts[i].name;
		account["password"] = accounts[i].password;
		account["roster"] = accounts[i].roster;
		account["favourite"] = accounts[i].favourite;
		listed.push_back(account);
	}

	nlohmann::json file;
	file["version"] = kFileVersion;
	file["accounts"] = listed;
	// Indented, because a file nobody is asked to edit is still a file somebody
	// will have to read when something has gone wrong with it.
	return file.dump(2);
}

bool AccountStore::FromJson(const std::string& text) {
	// Parsed without exceptions: a file a player has managed to corrupt is an
	// answer to give back, not a throw to catch in a drawing path.
	nlohmann::json file = nlohmann::json::parse(text, NULL, false);
	if (file.is_discarded() || !file.is_object())
		return false;

	AccountStore read;
	nlohmann::json::const_iterator listed = file.find("accounts");
	// A file that lists nothing is a store with nothing in it, which is what a
	// player who has just forgotten their last account has.
	if (listed != file.end()) {
		if (!listed->is_array())
			return false;
		for (nlohmann::json::const_iterator it = listed->begin(); it != listed->end(); ++it) {
			if (!it->is_object())
				continue;
			// Trimmed here as well as in Save, because the name is what the
			// roster and the mark are then asked for by.
			std::string name = Trim(StringAt(*it, "name"));
			// Save applies the rules the store is here for: what is blank is
			// refused, and what is named twice is one account.
			if (!read.Save(name, StringAt(*it, "password")))
				continue;
			read.SetRoster(name, StringAt(*it, "roster"));
			read.SetFavourite(name, BoolAt(*it, "favourite"));
		}
	}

	accounts = read.accounts;
	return true;
}
