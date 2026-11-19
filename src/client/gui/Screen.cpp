#include "../../util/Mth.h"
#include "../../platform/time.h"
#include <cstdio>
#include <cmath>
#include "Screen.h"
#include "components/Button.h"
#include "components/TextBox.h"
#include "../Minecraft.h"
#include "../renderer/Tesselator.h"
#include "../sound/SoundEngine.h"
#include "../../platform/input/Keyboard.h"
#include "../../platform/input/Mouse.h"
#include "../renderer/Textures.h"

Screen::Screen()
:   passEvents(false),
	clickedButton(NULL),
	tabButtonIndex(0),
	width(1),
	height(1),
	minecraft(NULL),
	font(NULL)
{
}

void Screen::render( int xm, int ym, float a )
{
	for (unsigned int i = 0; i < buttons.size(); i++) {
		Button* button = buttons[i];
		button->render(minecraft, xm, ym);
	}

	// render any text boxes after buttons
	for (unsigned int i = 0; i < textBoxes.size(); i++) {
		TextBox* textbox = textBoxes[i];
		textbox->render(minecraft, xm, ym);
	}
}

void Screen::init( Minecraft* minecraft, int width, int height )
{
	//particles = /*new*/ GuiParticles(minecraft);
	this->minecraft = minecraft;
	this->font = minecraft->font;
	this->width = width;
	this->height = height;
	init();
	setupPositions();
	updateTabButtonSelection();
}

void Screen::init()
{
}

void Screen::setSize( int width, int height )
{
	this->width = width;
	this->height = height;
	setupPositions();
}

bool Screen::handleBackEvent( bool isDown )
{
	return false;
}

void Screen::updateEvents()
{
	if (passEvents)
		return;

	while (Mouse::next())
		mouseEvent();

	// Replay key presses and typed characters in their original order.
	// (Handling all keys first used to run e.g. Enter before the last few
	// characters typed in the same tick, truncating chat/commands.)
	for (;;) {
		const int textOrder = Keyboard::peekTextCharOrder();
		const bool hasKey   = Keyboard::hasNextKey();
		if (textOrder < 0 && !hasKey) break;
		if (textOrder >= 0 && (!hasKey || textOrder <= Keyboard::peekKeyIndex())) {
			Keyboard::nextTextChar();
			keyboardTextEvent();
		} else {
			Keyboard::next();
			keyboardEvent();
		}
		// a handler may have closed this screen
		if (minecraft->screen != this) break;
	}
}

void Screen::mouseEvent()
{
	const MouseAction& e = Mouse::getEvent();
	if (!e.isButton())
		return;

	if (Mouse::getEventButtonState()) {
		int xm = e.x * width / minecraft->width;
		int ym = e.y * height / minecraft->height - 1;
		mouseClicked(xm, ym, Mouse::getEventButton());
	} else {
		int xm = e.x * width / minecraft->width;
		int ym = e.y * height / minecraft->height - 1;
		mouseReleased(xm, ym, Mouse::getEventButton());
	}
}

void Screen::keyboardEvent()
{
	if (Keyboard::getEventKeyState()) {
		//if (Keyboard.getEventKey() == Keyboard.KEY_F11) {
		//    minecraft->toggleFullScreen();
		//    return;
		//}
		keyPressed(Keyboard::getEventKey());
	}
}
void Screen::keyboardTextEvent()
{
	keyboardNewChar(Keyboard::getChar());
}
void Screen::renderBackground()
{
	renderBackground(0);
}

void Screen::renderBackground( int vo )
{
	if (minecraft->isLevelGenerated()) {
		fillGradient(0, 0, width, height, 0xc0101010, 0xd0101010);
	} else {
		renderPanorama(panoramaDim());
	}
}

// Rotating cube panorama like the 0.9.0 / Java title screen.
void Screen::renderPanorama(float dim)
{
	Textures* tx = minecraft->textures;
	TextureId faces[6];
	for (int i = 0; i < 6; ++i) {
		char name[64];
		sprintf(name, "gui/background/panorama_%d.png", i);
		faces[i] = tx->loadSmoothClampedTexture(name);
		if (!Textures::isTextureIdValid(faces[i])) { renderDirtTiles(0); return; }
	}

	const GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean cull  = glIsEnabled(GL_CULL_FACE);
	glDisable2(GL_DEPTH_TEST);
	glDisable2(GL_CULL_FACE);
	glDisable2(GL_FOG);
	glDisable2(GL_ALPHA_TEST);
	glDisable2(GL_BLEND);
	glEnable2(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix2();
	glLoadIdentity2();
	gluPerspective(120.0f, (float)minecraft->width / (float)Mth::Max(1, minecraft->height), 0.05f, 10.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix2();
	glLoadIdentity2();

	// slow spin + gentle bob, wrapped so the angle stays small (precision)
	const double t = std::fmod(getTimeSD() * 20.0, 400.0 * 2.0 * 3.14159265358979 * 9.0);
	glRotatef2(180.0f, 1, 0, 0);
	glRotatef2((float)(std::sin(t / 400.0) * 25.0 + 20.0), 1, 0, 0);
	glRotatef2((float)(-std::fmod(t * 0.1, 360.0)), 0, 1, 0);

	Tesselator& tess = Tesselator::instance;
	glColor4f2(1, 1, 1, 1);
	for (int k = 0; k < 6; ++k) {
		glPushMatrix2();
		if (k == 1) glRotatef2(90, 0, 1, 0);
		if (k == 2) glRotatef2(180, 0, 1, 0);
		if (k == 3) glRotatef2(-90, 0, 1, 0);
		if (k == 4) glRotatef2(90, 1, 0, 0);
		if (k == 5) glRotatef2(-90, 1, 0, 0);
		tx->bind(faces[k]);
		tess.begin();
		tess.color(0xffffff);
		tess.vertexUV(-1, -1, 1, 0, 0);
		tess.vertexUV( 1, -1, 1, 1, 0);
		tess.vertexUV( 1,  1, 1, 1, 1);
		tess.vertexUV(-1,  1, 1, 0, 1);
		tess.draw();
		glPopMatrix2();
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix2();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix2();
	if (depth) glEnable2(GL_DEPTH_TEST);
	if (cull) glEnable2(GL_CULL_FACE);

	// soft vignette towards the bottom + optional dimming for sub menus
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	const int a0 = (int)(dim * 255.0f);
	const int a1 = Mth::Min(255, a0 + 0x40);
	fillGradient(0, 0, width, height, (a0 << 24) | 0x101010, (a1 << 24) | 0x101010);
}

void Screen::renderDirtBackground( int vo )
{
	// outside a world, menus use the 0.9.0 panorama instead of dirt
	if (!minecraft->isLevelGenerated()) {
		renderPanorama(panoramaDim());
		return;
	}
	renderDirtTiles(vo);
}

void Screen::renderDirtTiles( int vo )
{
	//glDisable2(GL_LIGHTING);
	glDisable2(GL_FOG);
	Tesselator& t = Tesselator::instance;
	minecraft->textures->loadAndBindTexture("gui/background.png");
	glColor4f2(1, 1, 1, 1);
	float s = 32;
	float fvo = (float) vo;
	t.begin();
	t.color(0x404040);
	t.vertexUV(0, (float)height, 0, 0, height / s + fvo);
	t.vertexUV((float)width, (float)height, 0, width / s, (float)height / s + fvo);
	t.vertexUV((float)width, 0, 0, (float)width / s, 0 + fvo);
	t.vertexUV(0, 0, 0, 0, 0 + fvo);
	t.draw();
}

bool Screen::isPauseScreen()
{
	return true;
}

bool Screen::isErrorScreen()
{
	return false;
}

bool Screen::isInGameScreen()
{
	return true;
}

bool Screen::closeOnPlayerHurt() {
    return false;
}

void Screen::keyPressed( int eventKey )
{
	if (eventKey == Keyboard::KEY_ESCAPE) {
		minecraft->setScreen(NULL);
		//minecraft->grabMouse();
	}

	// pass key events to any text boxes first
	for (auto& textbox : textBoxes) {
		textbox->handleKey(eventKey);
	}

	if (minecraft->useTouchscreen())
		return;

	// "Tabbing" the buttons (walking with keys)
	const int tabButtonCount = tabButtons.size();
	if (!tabButtonCount)
		return;

	Options& o = minecraft->options;
	if (eventKey == o.keyMenuNext.key)
		if (++tabButtonIndex == tabButtonCount) tabButtonIndex = 0;
	if (eventKey == o.keyMenuPrevious.key)
		if (--tabButtonIndex == -1) tabButtonIndex = tabButtonCount-1;
	if (eventKey == o.keyMenuOk.key) {
		Button* button = tabButtons[tabButtonIndex];
		if (button->active) {
			minecraft->soundEngine->playUI("random.click", 1, 1);
			buttonClicked(button);
		}
	}

	updateTabButtonSelection();
}

void Screen::updateTabButtonSelection()
{
	if (minecraft->useTouchscreen())
		return;

	for (unsigned int i = 0; i < tabButtons.size(); ++i)
		tabButtons[i]->selected = (i == tabButtonIndex);
}

void Screen::mouseClicked( int x, int y, int buttonNum )
{
	if (buttonNum == MouseAction::ACTION_LEFT) {
		for (unsigned int i = 0; i < buttons.size(); ++i) {
			Button* button = buttons[i];
            //LOGI("Hit-testing button: %p\n", button);
			if (button->clicked(minecraft, x, y)) {
                button->setPressed();

                //LOGI("Hit-test successful: %p\n", button);
				clickedButton = button;
/*
#if !defined(ANDROID) && !defined(__APPLE__) //if (!minecraft->isTouchscreen()) {
					minecraft->soundEngine->playUI("random.click", 1, 1);
					buttonClicked(button);
#endif }
*/
			}
		}
	}

	// let textboxes see the click regardless
	for (auto& textbox : textBoxes) {
		textbox->mouseClicked(minecraft, x, y, buttonNum);
	}
}

void Screen::mouseReleased( int x, int y, int buttonNum )
{
	//LOGI("b_id: %d, (%p), text: %s\n", buttonNum, clickedButton, clickedButton?clickedButton->msg.c_str():"<null>");
	if (!clickedButton || buttonNum != MouseAction::ACTION_LEFT) return;

#if 1
//#if defined(ANDROID) || defined(__APPLE__) //if (minecraft->isTouchscreen()) {
		for (unsigned int i = 0; i < buttons.size(); ++i) {
			Button* button = buttons[i];
			if (clickedButton == button && button->clicked(minecraft, x, y)) {
				buttonClicked(button);
				minecraft->soundEngine->playUI("random.click", 1, 1);
				clickedButton->released(x, y);
			}
		}
# else //	} else {
		clickedButton->released(x, y);
#endif // }
	clickedButton = NULL;
}

bool Screen::renderGameBehind() {
	return true;
}

bool Screen::hasClippingArea( IntRectangle& out )
{
	return false;
}

void Screen::lostFocus() {
	for(std::vector<TextBox*>::iterator it = textBoxes.begin(); it != textBoxes.end(); ++it) {
		TextBox* tb = *it;
		tb->loseFocus(minecraft);
	}
}

void Screen::toGUICoordinate( int& x, int& y ) {
	x = x * width / minecraft->width;
	y = y * height / minecraft->height - 1;
}
