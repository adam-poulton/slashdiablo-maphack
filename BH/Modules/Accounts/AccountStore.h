#pragma once
#include <string>
#include <vector>

// One account as BH keeps it: the name and password to sign in with, the roster
// it is kept in, and whether it is a favourite. The password is held as typed,
// which ADR 0007 records the reasoning for.
struct Account {
	std::string name;
	std::string password;
	std::string roster;		// empty where the account is kept in none
	bool favourite;

	Account() : favourite(false) {}
};

// The accounts BH keeps and the rules about them.
//
// Knows nothing about drawing, nothing about the game, and nothing about the
// file it will be read from and written to. That is what lets the rules below be
// checked without a client running, which matters because they are the part of
// the feature that can silently lose a player's account.
class AccountStore {
	private:
		std::vector<Account> accounts;

		Account* Lookup(const std::string& name);

	public:
		// Keeps the account, or replaces the password of the one already kept
		// under that name. A name that is already kept keeps its roster and its
		// favourite mark: what the player did was retype a password, not describe
		// a new account.
		//
		// Answers whether anything was kept. A name or a password that is blank
		// once trimmed is not an account and is refused, since neither can sign
		// anyone in.
		bool Save(const std::string& name, const std::string& password);

		// Answers whether there was an account of that name to forget.
		bool Forget(const std::string& name);

		// The account of that name, or nothing. Told apart without regard to
		// case, as the realm tells them apart.
		const Account* Find(const std::string& name) const;

		bool SetRoster(const std::string& name, const std::string& roster);
		bool SetFavourite(const std::string& name, bool favourite);

		// The favourites, above the rosters. A favourite is not listed in its
		// roster as well: it is kept above them rather than inside one, so that
		// no account is drawn twice. The roster is remembered either way, and
		// dropping the favourite mark puts the account back in it.
		std::vector<Account> Favourites() const;

		// The rosters that have anything in them, named once each and in the
		// order they are drawn. Derived from the accounts rather than kept
		// alongside them, so there is no such thing as an empty roster to go
		// stale, and naming a roster is done by putting an account in it.
		std::vector<std::string> Rosters() const;

		// What is kept in one roster, favourites excluded. The accounts in no
		// roster are asked for with a blank name.
		std::vector<Account> InRoster(const std::string& roster) const;

		unsigned int Count() const { return (unsigned int)accounts.size(); }

		// The whole store as the text of the file it is kept in. Written with a
		// version so that a later shape, an encrypted one above all, can be told
		// from this one by something reading it rather than by guesswork.
		std::string ToJson() const;

		// Replaces what is kept with what the text describes, and answers whether
		// the text was the file's shape at all. A refused read leaves the store
		// exactly as it was, because half a player's accounts is worse than the
		// ones they had a moment ago.
		//
		// An account the text describes with nothing to sign in with is dropped
		// and the rest are kept: one unusable line is not worth the others.
		//
		// A line describes a whole account, so a name the text gives twice is one
		// account described by the later line, roster and mark included. That is
		// not what Save does with a name already kept, and the difference is the
		// act: a file says what an account is, where a player retyping a password
		// says only that.
		bool FromJson(const std::string& text);
};
