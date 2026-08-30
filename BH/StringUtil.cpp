#include "StringUtil.h"
#include <algorithm>
#include <string.h>

std::string Trim(std::string source) {
	source = source.erase(0, source.find_first_not_of(" "));
	source = source.erase(source.find_last_not_of(" ") + 1);
	source = source.erase(0, source.find_first_not_of("\t"));
	source = source.erase(source.find_last_not_of("\t") + 1);
	return source;
}
std::string ToLower(const std::string& text) {
	std::string result(text);
	std::transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}
std::string Join(const std::vector<std::string>& parts, const std::string& separator) {
	std::string result;
	for (unsigned int i = 0; i < parts.size(); i++) {
		if (i > 0)
			result += separator;
		result += parts[i];
	}
	return result;
}
bool IsTrue(const char *str) {
	return (_stricmp(str, "1") == 0 || _stricmp(str, "y") == 0 || _stricmp(str, "yes") == 0 || _stricmp(str, "true") == 0);
}
bool StringToBool(std::string str) {
	return IsTrue(str.c_str());
}
