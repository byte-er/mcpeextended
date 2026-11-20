#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__

#include "../../Screen.h"
#include "../../components/LargeImageButton.h"
#include "../../components/TextBox.h"
#include "../../components/Button.h"

class BuyButton: public ImageButton {
	typedef ImageButton super;
public:
	BuyButton(int id);
	void render(Minecraft* minecraft, int xm, int ym);
};


namespace Touch {

/** 0.9.0's flat button (spritesheet.png nine-patch), used by the title screen. */
class FlatButton: public Button {
	typedef Button super;
public:
	FlatButton(int id, const std::string& msg);
protected:
	void renderBg(Minecraft* minecraft, int xm, int ym);
};

class ToolsButton: public FlatButton {
	typedef FlatButton super;
public:
	ToolsButton(int id);
	void render(Minecraft* minecraft, int xm, int ym);
};

class StartMenuScreen: public Screen
{
public:
	StartMenuScreen();
	virtual ~StartMenuScreen();
	
	void init();
	void setupPositions();
	
	void tick();
	void render(int xm, int ym, float a);
	float panoramaDim() { return 0.0f; }

	void buttonClicked(Button* button);
	virtual void mouseClicked(int x, int y, int buttonNum);
	bool handleBackEvent(bool isDown);
	bool isInGameScreen();
private:
	void _updateLicense();
	
	FlatButton bHost;
	ToolsButton bOptions;
	BuyButton bBuy;

	std::string copyright;
	std::string version;
	std::string splash;
	float logoX, logoY, logoW, logoH;
};
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__*/
