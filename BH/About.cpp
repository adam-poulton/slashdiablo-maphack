#include "About.h"
#include "BH.h"
#include "Constants.h"
#include "D2Version.h"

namespace About {
	std::string Version() {
		return BH_VERSION;
	}

	std::string Branch() {
		return "planqi Resurgence/Slash branch";
	}

	std::vector<std::string> Lines() {
		std::vector<std::string> lines;
		lines.push_back(Version() + " (" + Branch() + ")");
		lines.push_back(std::string("Diablo II ") + D2Version::GetHumanReadableVersion());
		// Only worth saying when it is, since its absence is the normal case.
		if (BH::cGuardLoaded)
			lines.push_back("cGuard loaded");
		return lines;
	}
};
