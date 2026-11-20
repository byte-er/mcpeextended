#include "OptionsScreen.h"
#include "../../skin/PlayerSkin.h"

#include "StartMenuScreen.h"
#include "UsernameScreen.h"
#include "DialogDefinitions.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "CreditsScreen.h"

#include "../components/OptionsPane.h"
#include "../components/ImageButton.h"
#include "../components/OptionsGroup.h"

#include "../../renderer/Textures.h"
#include "../../renderer/gles.h"
#include "../Gui.h"
#include "../../../platform/input/Mouse.h"
#include <cstdlib>

namespace {
// Square tab with an icon from touchgui2.png, drawn with 0.9.0's flat
// button sprite (spritesheet.png); the selected tab looks pressed.
class IconTab : public Button {
public:
	IconTab(int id, int u, int v, int w, int h) : Button(id, ""), u(u), v(v), iw(w), ih(h) { width = height = 32; }
	void render(Minecraft* minecraft, int xm, int ym) {
		if (!visible) return;
		const bool down = selected || (_currentlyDown && isInside(xm, ym));
		minecraft->textures->loadAndBindTexture("gui/spritesheet.png");
		if (down) glColor4f2(0.72f, 0.72f, 0.72f, 1); else glColor4f2(1, 1, 1, 1);
		blitNineSlice((float)x, (float)y, (float)width, (float)height, 0, 0, 16, 16, 4, 128);
		minecraft->textures->loadAndBindTexture("gui/touchgui2.png");
		const float s = height * 0.72f / (float)(iw > ih ? iw : ih);
		const float w = iw * s, h = ih * s;
		blit(x + (width - w) * 0.5f, y + (height - h) * 0.5f, u, v, w, h, iw, ih);
		glColor4f2(1, 1, 1, 1);
	}
private:
	int u, v, iw, ih;
};
}

OptionsScreen::OptionsScreen()
	: btnClose(NULL),
	bHeader(NULL),
	btnChangeUsername(NULL),
	btnCredits(NULL),
	btnSkin(NULL),
	currentOptionPane(NULL),
	selectedCategory(0),
	paneScroll(0), scrollTouch(false), scrolling(false),
	scrollDownX(0), scrollDownY(0), scrollStart(0) {
}

OptionsScreen::~OptionsScreen() {

	if (btnClose != NULL) {
		delete btnClose;
		btnClose = NULL;
	}

	if (bHeader != NULL) {
		delete bHeader;
		bHeader = NULL;
	}

	if (btnChangeUsername != NULL) {
		delete btnChangeUsername;
		btnChangeUsername = NULL;
	}

	delete btnSkin;
	btnSkin = NULL;
	if (btnCredits != NULL) {
		delete btnCredits;
		btnCredits = NULL;
	}

	for (std::vector<Button*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		if (*it != NULL) {
			delete* it;
			*it = NULL;
		}
	}

	for (std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		if (*it != NULL) {
			delete* it;
			*it = NULL;
		}
	}

	categoryButtons.clear();
}

void OptionsScreen::init() {

	bHeader = new Touch::THeader(0, "Options");

	btnClose = new Touch::TButton(1, "Back");

	// 0.9.0 tabs: Game, Controls, Graphics, Sound, Account (icons from touchgui2.png)
	categoryButtons.push_back(new IconTab(2, 138, 5, 21, 18));
	categoryButtons.push_back(new IconTab(3, 109, 3, 22, 19));
	categoryButtons.push_back(new IconTab(4, 136, 33, 24, 20));
	categoryButtons.push_back(new IconTab(5, 109, 30, 22, 23));
	categoryButtons.push_back(new IconTab(6, 43, 8, 19, 22));

	btnChangeUsername = new Button(10, "Username");
	btnCredits = new Button(11, "Credits");
	btnSkin = new Button(12, PlayerSkin::label(minecraft->options.skin));

	buttons.push_back(bHeader);
	buttons.push_back(btnClose);
	buttons.push_back(btnChangeUsername);
	buttons.push_back(btnCredits);
	buttons.push_back(btnSkin);

	for (std::vector<Button*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		buttons.push_back(*it);
		tabButtons.push_back(*it);
	}

	generateOptionScreens();
	// start with first category selected
	selectCategory(0);
}

void OptionsScreen::setupPositions() {

	// 0.9.0: full-width title bar with Back inset on the left, square icon tabs below
	btnClose->width = 60;
	btnClose->height = 28;
	btnClose->x = 3;
	btnClose->y = 3;

	bHeader->x = 0;
	bHeader->y = 0;
	bHeader->width = width;
	bHeader->height = btnClose->height + 6;
	bHeader->xText = width / 2;

	int ty = bHeader->height + 5;
	// square tabs, as big as 0.9.0's but all five must fit the height
	int tabSize = 32;
	if (!categoryButtons.empty()) {
		const int fit = (height - ty - 3) / (int)categoryButtons.size() - 3;
		if (fit < tabSize) tabSize = fit < 16 ? 16 : fit;
	}
	for (std::vector<Button*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		(*it)->width = (*it)->height = tabSize;
		(*it)->x = 3;
		(*it)->y = ty;
		ty += (*it)->height + 3;
	}
	const int buttonHeight = 26;

	// Username button (bottom-left)
	if (btnChangeUsername != NULL) {

		btnChangeUsername->width = 80;
		btnChangeUsername->height = buttonHeight;

		btnChangeUsername->x = categoryButtons.empty() ? 0 : categoryButtons[0]->x + categoryButtons[0]->width + 8;
		btnChangeUsername->y = height - btnChangeUsername->height;
	}


	// Credits button (bottom-right)
	if (btnCredits != NULL) {

		btnCredits->width = btnChangeUsername->width;
		btnCredits->height = btnChangeUsername->height;

		btnCredits->x = width - btnCredits->width;
		btnCredits->y = height - btnCredits->height;
	}

	// Skin button (left of Credits)
	if (btnSkin != NULL && btnCredits != NULL) {
		btnSkin->width = btnCredits->width;
		btnSkin->height = btnCredits->height;
		btnSkin->x = btnCredits->x - btnSkin->width - 4;
		btnSkin->y = btnCredits->y;
	}

	layoutPanes();

	// don't override user selection on resize
}


int OptionsScreen::paneTop() const {
	return bHeader ? bHeader->height : 0;
}

void OptionsScreen::layoutPanes() {
	for (std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		if (categoryButtons.size() > 0 && categoryButtons[0] != NULL) {
			(*it)->x = categoryButtons[0]->x + categoryButtons[0]->width + 4;
			(*it)->y = paneTop() - (*it == currentOptionPane ? (int)paneScroll : 0);
			(*it)->width = width - (*it)->x;
			(*it)->setupPositions();
		}
	}
}

float OptionsScreen::maxPaneScroll() {
	if (!currentOptionPane) return 0;
	// the pane's height is its bottom edge (see OptionsPane::setupPositions)
	const float contentH = (float)(currentOptionPane->height - currentOptionPane->y);
	const float viewH = (float)(height - paneTop() - 12); // a little room after the last item
	return contentH > viewH ? contentH - viewH : 0.0f;
}

void OptionsScreen::render(int xm, int ym, float a) {

	renderBackground();

	// light strip behind the tabs (0.9.0)
	if (!categoryButtons.empty()) {
		const int sx = categoryButtons[0]->x + categoryButtons[0]->width + 3;
		fill(0, bHeader->height, sx, height, 0xffbbb0aa);
		fill(sx - 1, bHeader->height, sx, height, 0xff534950);
		fill(0, bHeader->height, sx - 1, bHeader->height + 1, 0xffd4cdc8);
	}

	super::render(xm, ym, a);

	int xmm = xm * width / minecraft->width;
	int ymm = ym * height / minecraft->height - 1;

	// drag to scroll once the finger has clearly moved vertically
	if (scrollTouch && Mouse::isButtonDown(MouseAction::ACTION_LEFT)) {
		int mx = Mouse::getX(), my = Mouse::getY();
		toGUICoordinate(mx, my);
		if (!scrolling && std::abs(my - scrollDownY) > 8 && std::abs(my - scrollDownY) > std::abs(mx - scrollDownX)) {
			scrolling = maxPaneScroll() > 0;
			// cancel what the press started (a slider drag)
			if (scrolling && currentOptionPane) currentOptionPane->mouseReleased(minecraft, -1000, -1000, 0);
		}
		if (scrolling) {
			paneScroll = scrollStart - (my - scrollDownY);
			const float m = maxPaneScroll();
			if (paneScroll < 0) paneScroll = 0;
			if (paneScroll > m) paneScroll = m;
			layoutPanes();
		}
	} else {
		scrollTouch = scrolling = false;
	}

	if (currentOptionPane != NULL) {
		// keep the pane below the title bar while scrolled
		const float gs = Gui::GuiScale;
		glEnable2(GL_SCISSOR_TEST);
		glScissor(0, 0, minecraft->width, (int)(minecraft->height - paneTop() * gs));
		currentOptionPane->render(minecraft, xmm, ymm);
		glDisable2(GL_SCISSOR_TEST);
	}
}

void OptionsScreen::removed() {
}

void OptionsScreen::buttonClicked(Button* button) {

	if (button == btnClose) {

		minecraft->options.save();
		// opened from the pause menu: go back there instead of the title screen
		minecraft->screenChooser.setScreen(minecraft->level ? SCREEN_PAUSE : SCREEN_STARTMENU);

	}
	else if (button == btnChangeUsername) {

		minecraft->options.save();
		minecraft->setScreen(new UsernameScreen());

	}
	else if (button->id > 1 && button->id < 2 + (int)categoryButtons.size()) {

		int categoryButton = button->id - categoryButtons[0]->id;
		selectCategory(categoryButton);

	}
	else if (button == btnSkin) {
		PlayerSkin::cycle(minecraft);
		btnSkin->msg = PlayerSkin::label(minecraft->options.skin);
	}
	else if (button == btnCredits) {

		minecraft->setScreen(new CreditsScreen());

	}
}

void OptionsScreen::selectCategory(int index) {

	int currentIndex = 0;

	for (std::vector<Button*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {

		if (index == currentIndex)
			(*it)->selected = true;
		else
			(*it)->selected = false;

		currentIndex++;
	}

	if (index < (int)optionPanes.size())
		currentOptionPane = optionPanes[index];
	selectedCategory = index;
	paneScroll = 0;
	if (bHeader) layoutPanes();
	// account buttons live on the last (person) tab
	const bool account = index == (int)categoryButtons.size() - 1;
	Button* accountButtons[3] = { btnChangeUsername, btnSkin, btnCredits };
	for (int i = 0; i < 3; ++i) if (accountButtons[i]) { accountButtons[i]->visible = account; accountButtons[i]->active = account; }
}

void OptionsScreen::generateOptionScreens() {

	for (int i = 0; i < 5; ++i)
		optionPanes.push_back(new OptionsPane());

	// Game
	optionPanes[0]->createOptionsGroup("options.group.game")
		.addOptionItem(&Options::Option::DIFFICULTY, minecraft)
		.addOptionItem(&Options::Option::SERVER_VISIBLE, minecraft)
		.addOptionItem(&Options::Option::THIRD_PERSON, minecraft)
		.addOptionItem(&Options::Option::GUI_SCALE, minecraft);

	// Controls
	optionPanes[1]->createOptionsGroup("options.group.controls")
		.addOptionItem(&Options::Option::SENSITIVITY, minecraft)
		.addOptionItem(&Options::Option::INVERT_MOUSE, minecraft)
		.addOptionItem(&Options::Option::PC_UI, minecraft);

	// Graphics (0.9.0's four first, then this game's extras)
	optionPanes[2]->createOptionsGroup("options.group.graphics")
		.addOptionItem(&Options::Option::RENDER_DISTANCE, minecraft)
		.addOptionItem(&Options::Option::GRAPHICS, minecraft)
		.addOptionItem(&Options::Option::BEAUTIFUL_SKIES, minecraft)
		.addOptionItem(&Options::Option::ANIMATED_WATER, minecraft)
		.addOptionItem(&Options::Option::FOV, minecraft)
		.addOptionItem(&Options::Option::AMBIENT_OCCLUSION, minecraft)
		.addOptionItem(&Options::Option::VIEW_BOBBING, minecraft)
		.addOptionItem(&Options::Option::LOD_TERRAIN, minecraft)
		.addOptionItem(&Options::Option::ANAGLYPH, minecraft)
		.addOptionItem(&Options::Option::LIMIT_FRAMERATE, minecraft)
		.addOptionItem(&Options::Option::VSYNC, minecraft);

	// Sound
	optionPanes[3]->createOptionsGroup("options.group.sound")
		.addOptionItem(&Options::Option::MUSIC, minecraft)
		.addOptionItem(&Options::Option::SOUND, minecraft);

	// Account: username, skin, credits buttons
	optionPanes[4]->createOptionsGroup("options.group.mojang");
}

void OptionsScreen::mouseClicked(int x, int y, int buttonNum) {
	if (y > paneTop() && currentOptionPane && x >= currentOptionPane->x) {
		scrollTouch = true;
		scrolling = false;
		scrollDownX = x; scrollDownY = y;
		scrollStart = paneScroll;
	}

	if (currentOptionPane != NULL)
		currentOptionPane->mouseClicked(minecraft, x, y, buttonNum);

	super::mouseClicked(x, y, buttonNum);
}

void OptionsScreen::mouseReleased(int x, int y, int buttonNum) {
	if (scrolling) { scrollTouch = scrolling = false; return; } // a scroll, not a tap
	scrollTouch = false;

	if (currentOptionPane != NULL)
		currentOptionPane->mouseReleased(minecraft, x, y, buttonNum);

	super::mouseReleased(x, y, buttonNum);
}

void OptionsScreen::tick() {

	if (currentOptionPane != NULL)
		currentOptionPane->tick(minecraft);

	super::tick();
}