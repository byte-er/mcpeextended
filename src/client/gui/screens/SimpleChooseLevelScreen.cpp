#include "SimpleChooseLevelScreen.h"
#include "../../../util/Mth.h"
#include "../Font.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../components/Button.h"
#include "../components/ImageButton.h"
#include "../../Minecraft.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../platform/input/Keyboard.h"

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:   bHeader(0),
    bGamemode(0),
    bBack(0),
    bCreate(0),
    levelName(levelName),
    hasChosen(false),
    gamemode(GameType::Survival),
    tLevelName(0, "World name"),
    tSeed(1, "World seed")
{
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
    if (bHeader) delete bHeader;
    delete bGamemode;
    delete bBack;
    delete bCreate;
}

void SimpleChooseLevelScreen::init()
{
    // header + close button
    bHeader = new Touch::THeader(0, "Create World");
    // create the back/X button as ImageButton like CreditsScreen
    bBack = new ImageButton(2, "");
    {
        ImageDef def;
        def.name = "gui/touchgui.png";
        def.width = 34;
        def.height = 26;
        def.setSrc(IntRectangle(150, 0, (int)def.width, (int)def.height));
        bBack->setImageDef(def, true);
    }
    if (minecraft->useTouchscreen()) {
        bGamemode = new Touch::TButton(1, "Survival mode");
        bCreate  = new Touch::TButton(3, "Create");
    } else {
        bGamemode = new Button(1, "Survival mode");
        bCreate  = new Button(3, "Create");
    }

    buttons.push_back(bHeader);
    buttons.push_back(bBack);
    buttons.push_back(bGamemode);
    buttons.push_back(bCreate);

    tabButtons.push_back(bGamemode);
    tabButtons.push_back(bBack);
    tabButtons.push_back(bCreate);

    textBoxes.push_back(&tLevelName);
    textBoxes.push_back(&tSeed);
}

void SimpleChooseLevelScreen::setupPositions()
{
    int buttonHeight = bBack->height;

    // position back button in upper-right
    bBack->x = width - bBack->width;
    bBack->y = 0;

    // header occupies remaining top bar
    if (bHeader) {
        bHeader->x = 0;
        bHeader->y = 0;
        bHeader->width = width - bBack->width;
        bHeader->height = buttonHeight;
    }

    // layout the form elements below the header
    int centerX = width / 2;
    const int padding = 5;

    tLevelName.width = tSeed.width = 200;
    tLevelName.x = centerX - tLevelName.width / 2;
    tLevelName.y = buttonHeight + Font::DefaultLineHeight + 8;

    tSeed.x = tLevelName.x;
    tSeed.y = tLevelName.y + tLevelName.height + Font::DefaultLineHeight + 6;

    // Mode + create stacked when there is room, otherwise side by side, so
    // nothing overlaps the mode description (used to on landscape phones).
    bGamemode->width = 140;
    bCreate->width = 100;
    const int rowY = tSeed.y + tSeed.height + 8;
    const int descH = Font::DefaultLineHeight + 4;
    const int stackedNeed = rowY + bGamemode->height + descH + bCreate->height + padding;
    if (stackedNeed <= height) {
        bGamemode->x = centerX - bGamemode->width / 2;
        bGamemode->y = rowY;
        bCreate->x = centerX - bCreate->width / 2;
        bCreate->y = Mth::Max(rowY + bGamemode->height + descH, height - bCreate->height - padding);
    } else {
        bGamemode->x = centerX - (bGamemode->width + bCreate->width + padding) / 2;
        bGamemode->y = rowY;
        bCreate->x = bGamemode->x + bGamemode->width + padding;
        bCreate->y = rowY;
    }
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

    const char* str = NULL;
    if (gamemode == GameType::Survival) {
        str = "Mobs, health and gather resources";
    } else if (gamemode == GameType::Creative) {
        str = "Unlimited resources and flying";
    }
    if (str) {
        drawCenteredString(minecraft->font, str, width/2, bGamemode->y + bGamemode->height + 4, 0xffcccccc);
    }

    drawString(minecraft->font, "World name:", tLevelName.x, tLevelName.y - Font::DefaultLineHeight - 2, 0xffcccccc);
    drawString(minecraft->font, "World seed:", tSeed.x, tSeed.y - Font::DefaultLineHeight - 2, 0xffcccccc);

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

    if (button == bGamemode) {
        gamemode ^= 1;
        bGamemode->msg = (gamemode == GameType::Survival) ? "Survival mode" : "Creative mode";
        return;
    }

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
        if (levelName.empty()) levelName = "My World"; // blank names showed up as "."
        std::string levelId = getUniqueLevelName(levelName);
        LevelSettings settings(seed, gamemode);
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
