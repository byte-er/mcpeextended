#include "SimpleChooseLevelScreen.h"
#include "../../../util/Mth.h"
#include "../Font.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../components/Button.h"
#include "../components/ImageButton.h"
#include "../components/NinePatch.h"
#include "../../renderer/Tesselator.h"
#include "../../Minecraft.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../platform/input/Keyboard.h"

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:   bBack(0),
    bCreate(0),
    panel(0),
    panelX(8),
    panelY(30),
    hasChosen(false),
    levelName(levelName),
    gamemode(GameType::Survival),
    worldType(LevelSettings::WORLD_INFINITE),
    tLevelName(0, "My World"),
    tSeed(1, "")
{
    bWorldType[0] = bWorldType[1] = bWorldType[2] = 0;
    bMode[0] = bMode[1] = 0;
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
    delete bBack;
    delete bCreate;
    for (int i = 0; i < 3; ++i) delete bWorldType[i];
    for (int i = 0; i < 2; ++i) delete bMode[i];
    delete panel;
}

Button* SimpleChooseLevelScreen::makeButton(int id, const std::string& label)
{
    if (minecraft->useTouchscreen()) return new Touch::TButton(id, label);
    return new Button(id, label);
}

void SimpleChooseLevelScreen::init()
{
    bBack   = makeButton(2, "Back");
    bCreate = makeButton(3, "Create World!");
    bWorldType[0] = makeButton(10, "Old");
    bWorldType[1] = makeButton(11, "Infinite");
    bWorldType[2] = makeButton(12, "Flat");
    bMode[0] = makeButton(20, "Survival");
    bMode[1] = makeButton(21, "Creative");

    buttons.push_back(bBack);
    for (int i = 0; i < 3; ++i) buttons.push_back(bWorldType[i]);
    for (int i = 0; i < 2; ++i) buttons.push_back(bMode[i]);
    buttons.push_back(bCreate);

    tabButtons.push_back(bBack);
    for (int i = 0; i < 3; ++i) tabButtons.push_back(bWorldType[i]);
    for (int i = 0; i < 2; ++i) tabButtons.push_back(bMode[i]);
    tabButtons.push_back(bCreate);

    textBoxes.push_back(&tLevelName);
    textBoxes.push_back(&tSeed);
    if (tLevelName.text.empty()) tLevelName.text = "My World";

    NinePatchFactory builder(minecraft->textures, "gui/spritesheet.png");
    panel = builder.createSymmetrical(IntRectangle(0, 0, 16, 16), 4, 4);

    updateToggles();
}

// button order on screen -> LevelSettings::WorldType
static const int WORLD_TYPE_FOR_BUTTON[3] = { LevelSettings::WORLD_OLD, LevelSettings::WORLD_INFINITE, LevelSettings::WORLD_FLAT };

// Selected toggles are drawn "pressed" (inactive) like 0.9.0
void SimpleChooseLevelScreen::updateToggles()
{
    for (int i = 0; i < 3; ++i) bWorldType[i]->active = (WORLD_TYPE_FOR_BUTTON[i] != worldType);
    bMode[0]->active = (gamemode != GameType::Survival);
    bMode[1]->active = (gamemode != GameType::Creative);
}

void SimpleChooseLevelScreen::setupPositions()
{
    // header bar: Back on the left, title in the middle
    bBack->width = 54;
    bBack->height = 20;
    bBack->x = 3;
    bBack->y = 3;
    const int headerH = 26;

    // panel: full width on phones, capped and centred on big screens /
    // small GUI scales so buttons never get wider than their texture
    const int pw = Mth::Min(width - 16, 320);
    const int px = (width - pw) / 2, py = headerH + 4;
    panelX = px; panelY = py;

    const int lh = Font::DefaultLineHeight;
    const int colW = (pw - 24) / 2;
    tLevelName.x = px + 8;
    tLevelName.y = py + 6 + lh + 2;
    tLevelName.width = colW;
    tSeed.x = tLevelName.x + colW + 8;
    tSeed.y = tLevelName.y;
    tSeed.width = colW;

    const int bh = 18;
    int y = tLevelName.y + tLevelName.height + lh + 8;
    const int bw = Mth::Min(84, (pw - 16 - 16) / 3);
    for (int i = 0; i < 3; ++i) {
        bWorldType[i]->width = bw;
        bWorldType[i]->height = bh;
        bWorldType[i]->x = px + 8 + i * (bw + 8);
        bWorldType[i]->y = y;
    }
    y += bh + lh + 6;
    for (int i = 0; i < 2; ++i) {
        bMode[i]->width = bw;
        bMode[i]->height = bh;
        bMode[i]->x = px + 8 + i * (bw + 8);
        bMode[i]->y = y;
    }
    bCreate->x = px + 8 + 2 * (bw + 8);
    bCreate->width = px + pw - 8 - bCreate->x;
    bCreate->height = bh + 4;
    bCreate->y = y - 2;
    // panel hugs its contents
    const int ph = Mth::Min(height - py - 4, bCreate->y + bCreate->height + 8 - py);
    if (panel) panel->setSize((float)pw, (float)ph);
}

void SimpleChooseLevelScreen::tick()
{
    // let any textboxes handle their own blinking/input
    for (auto* tb : textBoxes)
        tb->tick(minecraft);
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
    renderDirtBackground(0);
    glEnable2(GL_BLEND);
    glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // header bar
    fill(0, 0, width, 26, 0xff6b625e);
    fill(0, 25, width, 26, 0xff2c2724);
    drawCenteredString(minecraft->font, "Create a World", width / 2, 9, 0xffffffff);

    // light panel
    if (panel) {
        glColor4f2(1, 1, 1, 1);
        Tesselator& t = Tesselator::instance;
        panel->draw(t, (float)panelX, (float)panelY);
    }

    const int lh = Font::DefaultLineHeight;
    drawString(minecraft->font, "Name", tLevelName.x, tLevelName.y - lh - 1, 0xffffffff);
    drawString(minecraft->font, "Seed", tSeed.x, tSeed.y - lh - 1, 0xffffffff);
    drawString(minecraft->font, "Game Type", bWorldType[0]->x, bWorldType[0]->y - lh - 2, 0xffffffff);
    drawString(minecraft->font, "Game Mode", bMode[0]->x, bMode[0]->y - lh - 2, 0xffffffff);

    Screen::render(xm, ym, a);
    glDisable2(GL_BLEND);
}

// mouse clicks should also manage textbox focus explicitly
void SimpleChooseLevelScreen::mouseClicked(int x, int y, int buttonNum)
{
    if (buttonNum == MouseAction::ACTION_LEFT) {
        // determine if the click landed on either textbox or its label above
        auto hitBox = [&](TextBox &tb) {
            int top = tb.y;
            int bottom = tb.y + tb.height;
            int left = tb.x;
            int right = tb.x + tb.width;
            // include a bit of space above the box to cover the label
            top -= Font::DefaultLineHeight + 4;
            return x >= left && x < right && y >= top && y < bottom;
        };

        bool clickedLevel = hitBox(tLevelName);
        bool clickedSeed  = hitBox(tSeed);

        if (clickedLevel) {
            LOGI("SimpleChooseLevelScreen: level textbox clicked (%d,%d)\n", x, y);
            tLevelName.setFocus(minecraft);
            tSeed.loseFocus(minecraft);
        } else if (clickedSeed) {
            LOGI("SimpleChooseLevelScreen: seed textbox clicked (%d,%d)\n", x, y);
            tSeed.setFocus(minecraft);
            tLevelName.loseFocus(minecraft);
        } else {
            // click outside both fields -> blur both
            tLevelName.loseFocus(minecraft);
            tSeed.loseFocus(minecraft);
        }
    }

    // allow normal button and textbox handling too
    Screen::mouseClicked(x, y, buttonNum);
}

void SimpleChooseLevelScreen::buttonClicked( Button* button )
{
    if (hasChosen)
        return;

    for (int i = 0; i < 3; ++i)
        if (button == bWorldType[i]) { worldType = WORLD_TYPE_FOR_BUTTON[i]; updateToggles(); return; }
    if (button == bMode[0]) { gamemode = GameType::Survival; updateToggles(); return; }
    if (button == bMode[1]) { gamemode = GameType::Creative; updateToggles(); return; }

    if (button == bCreate) {
        int seed = getEpochTimeS();
        if (!tSeed.text.empty()) {
            std::string seedString = Util::stringTrim(tSeed.text);
            int tmpSeed;
            if (sscanf(seedString.c_str(), "%d", &tmpSeed) > 0) {
                seed = tmpSeed;
            } else {
                seed = Util::hashCode(seedString);
            }
        }
        std::string levelName = Util::stringTrim(tLevelName.text);
        if (levelName.empty()) levelName = "My World";
        std::string levelId = getUniqueLevelName(levelName);
        LevelSettings settings(seed, gamemode, worldType);
        minecraft->selectLevel(levelId, levelId, settings);
        minecraft->hostMultiplayer();
        minecraft->setScreen(new ProgressScreen());
        hasChosen = true;
        return;
    }

    if (button == bBack) {
        minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
    }
}

void SimpleChooseLevelScreen::keyPressed(int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE) {
        minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
        return;
    }
    // let base class handle navigation and text box keys
    Screen::keyPressed(eventKey);
}

void SimpleChooseLevelScreen::keyboardNewChar(char inputChar)
{
    // forward character input to focused textbox(s)
    for (auto* tb : textBoxes) tb->handleChar(inputChar);
}

bool SimpleChooseLevelScreen::handleBackEvent(bool isDown) {
	if (!isDown)
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	return true; 
}
