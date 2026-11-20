#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__

#include "../Screen.h"
#include <vector>
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include <string>

class DisconnectionScreen: public Screen
{
	typedef Screen super;
public:
	DisconnectionScreen(const std::string& msg)
	:	_msg(msg),
		_back(NULL)
	{}

	~DisconnectionScreen() {
		delete _back;
	}

	void init() {
		if (minecraft->useTouchscreen())
			_back = new Touch::TButton(1, "Ok");
		else
			_back = new Button(1, "Ok");

		buttons.push_back(_back);
		tabButtons.push_back(_back);

		_back->width = 128;
		_back->x = (width - _back->width) / 2;
		_back->y = height / 2;
	}

	void render( int xm, int ym, float a ) {
		renderBackground();
		super::render(xm, ym, a);

		// word-wrap long messages (server kick reasons)
		std::vector<std::string> lines;
		std::string line, word;
		const int maxW = width - 20;
		for (size_t i = 0; i <= _msg.size(); ++i) {
			char c = i < _msg.size() ? _msg[i] : ' ';
			if (c == ' ' || c == '\n') {
				std::string cand = line.empty() ? word : line + " " + word;
				if (!line.empty() && minecraft->font->width(cand) > maxW) { lines.push_back(line); line = word; }
				else line = cand;
				word.clear();
				if (c == '\n') { lines.push_back(line); line.clear(); }
			} else word += c;
		}
		if (!line.empty()) lines.push_back(line);
		int y = height / 2 - 14 - (int)lines.size() * 10;
		for (size_t i = 0; i < lines.size(); ++i, y += 10) {
			int center = (width - minecraft->font->width(lines[i])) / 2;
			minecraft->font->drawShadow(lines[i], (float)center, (float)y, 0xffffffff);
		}
	}

	void buttonClicked(Button* button) {
		if (button->id == _back->id) {
			minecraft->leaveGame();
		}
	}
	bool isInGameScreen() { return false; }

private:
	std::string _msg;
	Button* _back;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__*/
