#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__EaglerJoinScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__EaglerJoinScreen_H__

#include "../Screen.h"
#include "../components/Button.h"
#include <string>

/** Enter an Eaglercraft 1.8 server address (wss://...) and join it. */
class EaglerJoinScreen : public Screen
{
	typedef Screen super;
public:
	EaglerJoinScreen();
	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	void tick();
	void keyPressed(int eventKey);
	void keyboardNewChar(char inputChar);
	bool handleBackEvent(bool isDown);
	void mouseClicked(int x, int y, int button);
	void removed();
protected:
	void buttonClicked(Button* button);
private:
	void join();
	Button _btnJoin, _btnBack, _btnSkin;
	std::string _input;
	int _cursorBlink;
	int _boxX, _boxY, _boxW, _boxH;
};

/** Shows connection progress; Cancel goes back to the address screen. */
class EaglerStatusScreen : public Screen
{
	typedef Screen super;
public:
	EaglerStatusScreen();
	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	void tick();
	bool handleBackEvent(bool isDown);
protected:
	void buttonClicked(Button* button);
private:
	void cancel();
	Button _btnCancel;
	int _ticks;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__EaglerJoinScreen_H__*/
