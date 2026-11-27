#include "Session.h"
#include <ctime>
#include "../../BH.h"
#include "../../Common.h"
#include "../../Constants.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "../Settings/SettingsRegistry.h"
#include "SessionFormat.h"

using namespace SessionEvents;

namespace {
	// Where the logs are kept, beside the settings.
	const char* const kFolder = "BH_sessions\\";

	// How often the tokens are rewritten. The fastest of them is a clock that
	// shows seconds.
	const unsigned long long kPublishEveryMs = 250;

	// How far back "recent" looks.
	const unsigned long long kRecentWindowMs = 60 * 1000;

	// The tokens this module keeps, for AutomapInfo lines and run tracker
	// columns. Named apart from the run tracker's own, none of which they
	// replace.
	const char* const kTokens[] = {
		"SESSIONTIME", "SESSIONPLAYTIME", "SESSIONXPGAINED", "SESSIONXPPERSEC",
		"SESSIONXPPERSECLONG", "RECENTXPPERSEC", "SESSIONLEVELS", "SESSIONDEATHS",
	};

	std::string KeyFor(const GameProbe::Identity& who) {
		return who.account + "\n" + who.realm + "\n" + who.character;
	}
}

bool Session::enabled = true;
bool Session::keepHistory = true;

void Session::OnLoad() {
	LoadConfig();
	screenInfo = (ScreenInfo*)BH::moduleManager->Get("Screen Info");
	ClearTokens();

	Settings::AddHeading(GetName(), Settings::Category::Display, "Session");
	Settings::AddBool(GetName(), Settings::Category::Display, "Session Tracker",
		"Session tracker", &enabled,
		"Keeps track of a sitting on a character: time played, games, experience "
		"and how fast it is coming, and deaths.");
	Settings::AddBool(GetName(), Settings::Category::Display, "Keep Session History",
		"Keep session history", &keepHistory,
		"Writes each session to a file of its own in BH_sessions, beside the settings.",
		"Session Tracker");
}

void Session::LoadConfig() {
	BH::config->ReadBoolean("Session Tracker", enabled);
	BH::config->ReadBoolean("Keep Session History", keepHistory);
}

void Session::OnSettingsChanged(const vector<string>& keys) {
	std::lock_guard<std::mutex> held(lock);
	if (!enabled) {
		// A tracker switched off mid-game has seen the last of that game.
		std::vector<GameProbe::Seen> seen;
		probe.Leave(BHGetTickCount(), seen);
		Take(seen);
		ClearTokens();
	}
	if (current)
		Write(*current);
}

void Session::OnLoop() {
	if (!enabled)
		return;

	const unsigned long long tick = BHGetTickCount();
	std::lock_guard<std::mutex> held(lock);
	std::vector<GameProbe::Seen> seen;
	probe.Sample(tick, seen);
	Take(seen);
	PublishTokens(tick);
}

void Session::OnGameExit() {
	std::lock_guard<std::mutex> held(lock);
	std::vector<GameProbe::Seen> seen;
	probe.Leave(BHGetTickCount(), seen);
	Take(seen);
}

void Session::Take(const std::vector<GameProbe::Seen>& seen) {
	if (seen.empty())
		return;

	for (const GameProbe::Seen& one : seen) {
		if (std::holds_alternative<Joined>(one.fact))
			current = SessionFor(probe.GetIdentity(), one.tick);
		if (!current)
			continue;
		const unsigned long long at =
			one.tick > current->beganAt ? one.tick - current->beganAt : 0;
		Record(*current, { at, one.fact });
	}
	if (current)
		Write(*current);
}

Session::Live* Session::SessionFor(const GameProbe::Identity& who,
		unsigned long long tick) {
	std::unique_ptr<Live>& held = sessions[KeyFor(who)];
	if (held)
		return held.get();

	held.reset(new Live());
	held->beganAt = tick;
	held->written = 0;
	held->unwritable = false;

	Started fact;
	// The wall clock at the tick the session's clock starts from, which is a
	// moment ago by the time the player's stats have appeared.
	fact.startedAt = (long long)time(NULL) - (long long)((BHGetTickCount() - tick) / 1000);
	fact.account = who.account;
	fact.realm = who.realm;
	fact.character = who.character;
	fact.classId = who.classId;
	fact.hardcore = who.hardcore;
	fact.expansion = who.expansion;
	fact.version = VERSION;
	Record(*held, { 0, fact });
	return held.get();
}

void Session::Record(Live& session, const Event& event) {
	session.model.Apply(event);
	session.log.push_back(event);
}

void Session::Write(Live& session) {
	if (!keepHistory || session.unwritable || session.written == session.log.size())
		return;

	if (!session.file.IsOpen()) {
		const Started& header = session.model.Header();
		const time_t began = (time_t)header.startedAt;
		std::tm local = {};
		localtime_s(&local, &began);
		const std::string path = BH::path + kFolder +
			SessionFile::NameFor(local, header.character, GetCurrentProcessId());
		if (!session.file.Open(path)) {
			session.unwritable = true;
			return;
		}
	}

	for (; session.written < session.log.size(); session.written++)
		session.file.Append(session.log[session.written]);
	session.file.Flush();
}

void Session::PublishTokens(unsigned long long tick) {
	if (!screenInfo || !current || tick - publishedAt < kPublishEveryMs)
		return;
	publishedAt = tick;

	const SessionModel& model = current->model;
	const unsigned long long now = tick > current->beganAt ? tick - current->beganAt : 0;
	const long long net = (long long)model.Gained() - (long long)model.Lost();

	screenInfo->SetAutomapToken("SESSIONTIME", SessionFormat::Clock(now));
	screenInfo->SetAutomapToken("SESSIONPLAYTIME", SessionFormat::Clock(model.PlayedMs(now)));
	screenInfo->SetAutomapToken("SESSIONXPGAINED", std::to_string(net));
	screenInfo->SetAutomapToken("SESSIONXPPERSEC", SessionFormat::Rate(model.PlayedRate(now)));
	screenInfo->SetAutomapToken("SESSIONXPPERSECLONG", std::to_string(model.PlayedRate(now)));
	screenInfo->SetAutomapToken("RECENTXPPERSEC",
		SessionFormat::Rate(model.RecentRate(now, kRecentWindowMs)));
	screenInfo->SetAutomapToken("SESSIONLEVELS", std::to_string(model.LevelsGained()));
	screenInfo->SetAutomapToken("SESSIONDEATHS", std::to_string(model.Deaths()));
}

// A token with nothing in it is written as nothing, where one nobody has set is
// left on screen as its own name.
void Session::ClearTokens() {
	if (!screenInfo)
		return;
	for (const char* token : kTokens)
		screenInfo->SetAutomapToken(token, "");
	publishedAt = 0;
}
