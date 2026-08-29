#pragma once
#include <string>

/*
 * Small things done to strings, kept apart from the rest of Common.
 *
 * Nothing here touches the game. Common does, by way of what it includes, so
 * anything wanting to trim a string was reaching the whole of the game's entry
 * points to get it. These are what the item filter needs, and having them here
 * is what lets a rule be read without a client running.
 */

// Without the spaces and tabs at either end.
std::string Trim(std::string source);

std::string ToLower(const std::string& text);

// True for the several ways a setting can say yes.
bool IsTrue(const char *str);
bool StringToBool(std::string str);
