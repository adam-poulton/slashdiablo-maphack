#include "Accounts.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../StringUtil.h"
#include "../Settings/SettingsRegistry.h"
#include "LoginScreen.h"

using namespace Drawing;

// The size the window is, always. It is not resizable, so this is not a starting
// point to be grown from: it is what the panel is laid out to fit.
#define ACCOUNTS_WINDOW_WIDTH	210
#define ACCOUNTS_WINDOW_HEIGHT	280

// The file the accounts are kept in, beside the settings.
#define ACCOUNTS_FILE_NAME		"BH_accounts.json"

namespace {

// What the game's boxes hold, as the rest of BH says text.
std::string ReadControl(Control* box) {
	if (!box)
		return std::string();
	std::string said;
	for (unsigned int i = 0; i < (sizeof(box->wText) / sizeof(box->wText[0])); i++) {
		if (!box->wText[i])
			break;
		said += (char)box->wText[i];
	}
	return said;
}

// Who the realm says this client is signed in as, and nothing at all where it
// says nobody: on the login screen, out of a realm, or before a sign-in has been
// answered. What the realm itself holds rather than what was typed at it, so a
// sign-in that was refused claims no account.
std::string SignedInAccount() {
	BnetData* realm = p_D2LAUNCH_BnData ? *p_D2LAUNCH_BnData : NULL;
	if (!realm)
		return std::string();

	const char* said = realm->szAccountName;
	unsigned int length = 0;
	while (length < sizeof(realm->szAccountName) && said[length])
		length++;
	return std::string(said, length);
}

void WriteControl(Control* box, const std::string& text) {
	if (!box)
		return;
	std::wstring wide(text.begin(), text.end());
	D2WIN_SetControlText(box, (wchar_t*)wide.c_str());
}

}  // namespace

AccountsWindow::~AccountsWindow() {
	delete file;
	file = NULL;
}

void AccountsWindow::OnLoad() {
	Settings::AddBool(GetName(), Settings::Category::Lobby, ACCOUNTS_SETTING_KEY,
		"Account manager", &showPanel,
		"Lists the accounts you have kept, on the login screen: click one to sign "
		"in, right click one to label it, favourite it or forget it. Switched "
		"off, nothing is drawn there and nothing is read.");

	LoadConfig();

	file = new AccountFile(BH::path + ACCOUNTS_FILE_NAME);

	CreateUI("Accounts", "Accounts", ACCOUNTS_WINDOW_WIDTH, ACCOUNTS_WINDOW_HEIGHT);
	// The one window that is not drawn inside a game.
	GetUI()->SetVisibility(OutOfGame);

	// Wide enough for an account name and no wider. Nothing here grows usefully
	// with the window: the names are short, and a list of them read down rather
	// than across. Fixed also keeps the panel clear of the game's own boxes,
	// which is the whole reason it sits where it does.
	GetUI()->SetFixedSize(ACCOUNTS_WINDOW_WIDTH, ACCOUNTS_WINDOW_HEIGHT);

	// Nothing to search: what a player keeps here is a list they can see all of.
	SetFocusSearchOnOpen(false);

	panel = new AccountPanel(GetUI(), this);
	AddPanel(panel);

	// A window is remembered as collapsed unless it says otherwise, which for a
	// panel that appears by itself would mean a title bar to click open on every
	// launch. Read again with the other default, so a player who has never
	// touched it gets the panel and one who collapsed it keeps it collapsed.
	char remembered[20];
	GetPrivateProfileString("Accounts", "Minimized", "false", remembered, 20,
		std::string(BH::path + "UI.ini").c_str());
	GetUI()->SetMinimized(StringToBool(remembered));

	// The file is not read here. Reaching the login screen is what reads it, and
	// that happens before anything of it can be drawn, so a player who has the
	// panel switched off never has their accounts read at all.
}

// Deliberately not the base's, which would read a hotkey for the window's own
// toggle and write it to the config. Nothing summons this window, so there is no
// binding to offer; what a player chooses is whether it appears at all.
void AccountsWindow::LoadConfig() {
	BH::config->ReadBoolean(ACCOUNTS_SETTING_KEY, showPanel);
}

void AccountsWindow::Reread() {
	if (!file)
		return;
	file->Read(accounts);
	if (panel)
		panel->Refresh();
}

// Drawn on the login screen and nowhere else. The screen is worked out from the
// controls the game has built rather than from any address, so a screen that
// stops matching costs a panel that does not appear.
void AccountsWindow::OnOOGDraw() {
	if (!GetUI())
		return;

	// Switched off, the screen is not even asked about: nothing is drawn, nothing
	// is read from the file, and no claim is held.
	bool nowOnLoginScreen = showPanel &&
		FindLoginBoxes(*p_D2WIN_FirstControl).Found();
	if (nowOnLoginScreen != onLoginScreen) {
		onLoginScreen = nowOnLoginScreen;
		if (onLoginScreen) {
			// Another client may have kept an account since this one last looked,
			// and backing out to the login screen is what asks again.
			Reread();
		}
	}

	HoldSignedInAccount();

	Toggles[ACCOUNTS_TOGGLE_NAME].state = onLoginScreen;
	GetUI()->SetVisible(onLoginScreen);
	CheckOpenState();

	// Everything a window does with its panels every frame, which is the base's
	// and not this window's. Called from here rather than from OnDraw() because
	// this is the one window drawn outside a game.
	WindowModule::OnDraw();
}

// The claim follows the realm rather than the act of signing in, which is what
// lets an account typed into the game's own boxes be marked as in use alongside
// one clicked in the panel. It also means a sign-in that failed claims nothing,
// there being no account the realm says this client is.
void AccountsWindow::HoldSignedInAccount() {
	// Switched off, nothing of this feature runs, claims included. The login
	// screen holds no claim either, whatever the realm was last told: what it
	// says of a client sitting there is stale, and a stale claim is an account
	// the other clients are told to keep off for nothing.
	std::string signedIn = (showPanel && !onLoginScreen) ?
		SignedInAccount() : std::string();
	if (signedIn.empty()) {
		claim.Drop();
		return;
	}

	// Told apart without regard to case, as the realm tells account names apart,
	// so that the claim is not taken again every frame for the same account.
	if (!claim.Held() || ToLower(claim.Name()) != ToLower(Trim(signedIn)))
		claim.Take(signedIn);
}

bool AccountsWindow::InUse(const std::string& accountName) {
	return AccountClaim::InUse(accountName);
}

// Fills both boxes and presses return, which is what the player does. Only ever
// from a click on a row: ADR 0009.
bool AccountsWindow::SignIn(const Account& account) {
	LoginBoxes boxes = FindLoginBoxes(*p_D2WIN_FirstControl);
	if (!boxes.Found())
		return false;

	WriteControl(boxes.account, account.name);
	WriteControl(boxes.password, account.password);

	// Whatever of ours had the caret lets go of it first, or it would take the
	// return keystroke before the game ever saw it.
	if (Inputhook::current)
		Inputhook::current->SetFocused(false);

	HWND window = D2GFX_GetHwnd();
	if (!window)
		return false;
	// Posted rather than sent, so the screen is signed in from the game's own
	// message loop and not from the middle of a frame being drawn.
	PostMessage(window, WM_KEYDOWN, VK_RETURN, 0);
	PostMessage(window, WM_KEYUP, VK_RETURN, 0);

	// Nothing is claimed here. Whether this worked is the realm's answer to give,
	// and HoldSignedInAccount takes the claim once it has given it.
	return true;
}

std::string AccountsWindow::TypedAccountName() {
	return ReadControl(FindLoginBoxes(*p_D2WIN_FirstControl).account);
}

// How much of a password has been typed, never what it says. The panel draws a
// star for each character so the player can see it is being read, and has no
// use for the password itself.
unsigned int AccountsWindow::TypedPasswordLength() {
	return (unsigned int)ReadControl(FindLoginBoxes(*p_D2WIN_FirstControl).password).length();
}

// Kept from the game's own boxes rather than typed into ours, so that what is
// written is exactly what the game accepted.
bool AccountsWindow::SaveTyped() {
	LoginBoxes boxes = FindLoginBoxes(*p_D2WIN_FirstControl);
	if (!boxes.Found())
		return false;

	std::string name = ReadControl(boxes.account);
	std::string password = ReadControl(boxes.password);
	if (!file->Modify([&name, &password](AccountStore& store) {
			return store.Save(name, password);
		}))
		return false;

	Reread();
	return true;
}

bool AccountsWindow::SetRoster(const std::string& accountName, const std::string& roster) {
	if (!file->Modify([&accountName, &roster](AccountStore& store) {
			return store.SetRoster(accountName, roster);
		}))
		return false;
	Reread();
	return true;
}

bool AccountsWindow::SetFavourite(const std::string& accountName, bool favourite) {
	if (!file->Modify([&accountName, favourite](AccountStore& store) {
			return store.SetFavourite(accountName, favourite);
		}))
		return false;
	Reread();
	return true;
}

bool AccountsWindow::Forget(const std::string& accountName) {
	if (!file->Modify([&accountName](AccountStore& store) {
			return store.Forget(accountName);
		}))
		return false;
	Reread();
	return true;
}
