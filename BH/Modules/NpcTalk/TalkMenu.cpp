#include "TalkMenu.h"

namespace TalkMenu {

int Find(const std::vector<Line>& lines, unsigned int handler) {
	if (handler == 0)
		return -1;
	for (unsigned int i = 0; i < lines.size(); i++) {
		if (lines[i].selectable && lines[i].handler == handler)
			return (int)i;
	}
	return -1;
}

OncePerTalk::OncePerTalk() : talking(false), npcId(0), taken(false) {}

bool OncePerTalk::Take(bool nowTalking, unsigned int nowNpcId, bool offered) {
	if (nowTalking != talking || nowNpcId != npcId) {
		talking = nowTalking;
		npcId = nowNpcId;
		taken = false;
	}
	if (!talking || taken || !offered)
		return false;
	taken = true;
	return true;
}

void OncePerTalk::Reset() {
	*this = OncePerTalk();
}

}
