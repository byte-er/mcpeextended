#include "EaglerJoinScreen.h"
#include "../../Minecraft.h"
#include "../Font.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../AppPlatform.h"
#include "../../../network/eagler/EaglerClient.h"
#include "../../skin/PlayerSkin.h"

static const char* SKIN_NAMES[] = { "Skin: Steve", "Skin: Alex", "Skin: Custom (skin.png)" };

EaglerJoinScreen::EaglerJoinScreen()
:	_btnJoin(1, "Join Server"),
	_btnBack(2, "Back"),
	_btnSkin(3, ""),
	_cursorBlink(0),
	_boxX(0), _boxY(0), _boxW(200), _boxH(18)
{
}

void EaglerJoinScreen::init()
{
	_input = minecraft->options.eaglerServer;
	buttons.push_back(&_btnJoin);
	buttons.push_back(&_btnBack);
	buttons.push_back(&_btnSkin);
	tabButtons.push_back(&_btnJoin);
	tabButtons.push_back(&_btnSkin);
	tabButtons.push_back(&_btnBack);
	setupPositions();
}

void EaglerJoinScreen::setupPositions()
{
	_boxW = width - 40 < 240 ? width - 40 : 240;
	_boxX = (width - _boxW) / 2;
	_boxY = height / 2 - 30;
	_btnJoin.width = _btnBack.width = (_boxW - 8) / 2;
	_btnJoin.height = _btnBack.height = 20;
	_btnJoin.x = _boxX;
	_btnBack.x = _boxX + _boxW - _btnBack.width;
	_btnJoin.y = _btnBack.y = _boxY + 60;
	_btnSkin.width = _boxW;
	_btnSkin.height = 20;
	_btnSkin.x = _boxX;
	_btnSkin.y = _boxY + 32;
	int s = minecraft->options.skin;
	_btnSkin.msg = SKIN_NAMES[(s >= 0 && s <= 2) ? s : 0];
	_btnJoin.active = !_input.empty();
}

void EaglerJoinScreen::tick() { _cursorBlink++; }

void EaglerJoinScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_BACKSPACE) {
		if (!_input.empty()) _input.erase(_input.size() - 1, 1);
	} else if (eventKey == Keyboard::KEY_RETURN) {
		join();
		return;
	} else if (eventKey == Keyboard::KEY_ESCAPE) {
		buttonClicked(&_btnBack);
		return;
	}
	_btnJoin.active = !_input.empty();
}

void EaglerJoinScreen::keyboardNewChar(char c)
{
	if (_input.size() < 200 && c > 32 && c < 127) _input += c;
	_btnJoin.active = !_input.empty();
}

bool EaglerJoinScreen::handleBackEvent(bool isDown)
{
	if (!isDown) buttonClicked(&_btnBack);
	return true;
}

void EaglerJoinScreen::mouseClicked(int x, int y, int button)
{
	if (x >= _boxX && x <= _boxX + _boxW && y >= _boxY && y <= _boxY + _boxH)
		minecraft->platform()->showKeyboard();
	else
		super::mouseClicked(x, y, button);
}

void EaglerJoinScreen::removed()
{
	minecraft->platform()->hideKeyboard();
}

void EaglerJoinScreen::join()
{
	if (_input.empty()) return;
	minecraft->options.eaglerServer = _input;
	minecraft->options.save();
	minecraft->joinEaglerServer(_input);
}

void EaglerJoinScreen::buttonClicked(Button* button)
{
	if (button == &_btnJoin) join();
	else if (button == &_btnBack) minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
	else if (button == &_btnSkin) {
		minecraft->options.skin = (minecraft->options.skin + 1) % 3;
		minecraft->options.save();
		PlayerSkin::reload(minecraft);
		setupPositions();
	}
}

void EaglerJoinScreen::render(int xm, int ym, float a)
{
	renderBackground();
	const int cx = width / 2;
	drawCenteredString(font, "Join Eaglercraft Server", cx, _boxY - 40, 0xffffffff);
	drawString(font, "Server address", _boxX, _boxY - 12, 0xffa0a0a0);

	fill(_boxX - 1, _boxY - 1, _boxX + _boxW + 1, _boxY + _boxH + 1, 0xffa0a0a0);
	fill(_boxX, _boxY, _boxX + _boxW, _boxY + _boxH, 0xff000000);
	std::string shown = _input;
	// keep the end of long addresses visible
	while (!shown.empty() && font->width(shown) > _boxW - 10) shown.erase(0, 1);
	if ((_cursorBlink / 10) % 2 == 0) shown += '_';
	if (_input.empty() && (_cursorBlink / 10) % 2 != 0)
		font->draw("wss://eagarchy.wackify.dev", (float)(_boxX + 4), (float)(_boxY + 5), 0xff505050, false);
	else
		font->draw(shown, (float)(_boxX + 4), (float)(_boxY + 5), 0xffffffff, false);

	drawCenteredString(font, "Eaglercraft 1.8.9 servers. Overworld only.", cx, _btnJoin.y + 30, 0xff808080);
	drawCenteredString(font, "Your name: " + EaglerClient::sanitiseUsername(minecraft->options.username), cx, _btnJoin.y + 42, 0xff808080);
	super::render(xm, ym, a);
}

// ---------------------------------------------------------------------------

EaglerStatusScreen::EaglerStatusScreen()
:	_btnCancel(1, "Cancel"),
	_ticks(0)
{
}

void EaglerStatusScreen::init()
{
	buttons.push_back(&_btnCancel);
	tabButtons.push_back(&_btnCancel);
	setupPositions();
}

void EaglerStatusScreen::setupPositions()
{
	_btnCancel.width = 120;
	_btnCancel.height = 20;
	_btnCancel.x = (width - _btnCancel.width) / 2;
	_btnCancel.y = height / 2 + 30;
}

void EaglerStatusScreen::tick()
{
	++_ticks;
	EaglerClient* e = minecraft->eagler;
	if (!e) return;
	// in the world, placed by the server: start playing
	if (e->isTerrainReady() && minecraft->level && minecraft->isLevelGenerated())
		minecraft->setScreen(NULL);
}

void EaglerStatusScreen::cancel()
{
	if (minecraft->level) {
		minecraft->leaveGame();
	} else {
		delete minecraft->eagler;
		minecraft->eagler = NULL;
	}
	minecraft->setScreen(new EaglerJoinScreen());
}

bool EaglerStatusScreen::handleBackEvent(bool isDown)
{
	if (!isDown) cancel();
	return true;
}

void EaglerStatusScreen::buttonClicked(Button* button)
{
	if (button == &_btnCancel) cancel();
}

void EaglerStatusScreen::render(int xm, int ym, float a)
{
	renderBackground();
	const int cx = width / 2;
	std::string st = minecraft->eagler ? minecraft->eagler->getStatus() : std::string();
	if (st.empty()) st = "Downloading terrain...";
	std::string dots((_ticks / 8) % 4, '.');
	drawCenteredString(font, st, cx, height / 2 - 20, 0xffffffff);
	drawCenteredString(font, minecraft->options.eaglerServer, cx, height / 2 - 6, 0xff808080);
	drawCenteredString(font, dots, cx, height / 2 + 8, 0xffffffff);
	super::render(xm, ym, a);
}
