#include "TouchStartMenuScreen.h"
#include "../ProgressScreen.h"
#include "../OptionsScreen.h"
#include "../PauseScreen.h"
#include "../InvalidLicenseScreen.h"
//#include "BuyGameScreen.h"

#include "../../Font.h"
#include "../../components/SmallButton.h"
#include "../../components/ScrolledSelectionList.h"
#include "../../components/GuiElement.h"

#include "../../../Minecraft.h"
#include "../../../renderer/Tesselator.h"
#include "../../../renderer/Textures.h"
#include "../../../renderer/TextureData.h"
#include "../../../../SharedConstants.h"
#include "../../../../AppPlatform.h"
#include "../../../../LicenseCodes.h"
#include "../../../../util/Mth.h"

#include "../DialogDefinitions.h"
#include "../SimpleChooseLevelScreen.h"
#include "../Splashes.h"
#include "../../../renderer/gles.h"
#include "../../../../platform/time.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

//
// Buy Button implementation
//
BuyButton::BuyButton(int id)
:	super(id, "")
{
	ImageDef def;
	// Setup the source rectangle
	def.setSrc(IntRectangle(64, 182, 190, 55));
	def.width = 75;//rc.w / 3;
	def.height = 75 * (55.0f / 190.0f);//rc.h / 3;
	def.name = "gui/gui.png";

	setImageDef(def, true);
}

void BuyButton::render(Minecraft* minecraft, int xm, int ym) {
	glColor4f2(1, 1, 1, 1);
	bool hovered = active && (minecraft->useTouchscreen()? (xm >= x && ym >= y && xm < x + width && ym < y + height) : false);
	renderBg(minecraft, xm, ym);
	TextureId texId = (_imageDef.name.length() > 0)? minecraft->textures->loadAndBindTexture(_imageDef.name) : Textures::InvalidId;
	if ( Textures::isTextureIdValid(texId) ) {
		const ImageDef& d = _imageDef;
		Tesselator& t = Tesselator::instance;
		
		t.begin();
			if (!active)				t.color(0xff808080);
			else if (hovered||selected) t.color(0xffcccccc);
			//else						t.color(0xffe0e0e0);
			else t.color(0xffffffff);

			float hx = ((float) d.width) * 0.5f;
			float hy = ((float) d.height) * 0.5f;
			const float cx = ((float)x+d.x) + hx;
			const float cy = ((float)y+d.y) + hy;
			if (hovered) {
				hx *= 0.95f;
				hy *= 0.95f;
			}

			const TextureData* td = minecraft->textures->getTemporaryTextureData(texId);
			const IntRectangle* src = _imageDef.getSrc();
			if (td != NULL && src != NULL) {
				float u0 = (src->x) / (float)td->w;
				float u1 = (src->x+src->w) / (float)td->w;
				float v0 = (src->y) / (float)td->h;
				float v1 = (src->y+src->h) / (float)td->h;
				t.vertexUV(cx-hx, cy-hy, blitOffset, u0, v0);
				t.vertexUV(cx-hx, cy+hy, blitOffset, u0, v1);
				t.vertexUV(cx+hx, cy+hy, blitOffset, u1, v1);
				t.vertexUV(cx+hx, cy-hy, blitOffset, u1, v0);
			}
		t.draw();
	}
}

namespace Touch {

FlatButton::FlatButton(int id, const std::string& msg)
:	super(id, msg)
{
	width = 66;
	height = 26;
}

void FlatButton::renderBg(Minecraft* minecraft, int xm, int ym) {
	const bool down = active && _currentlyDown && xm >= x && ym >= y && xm < x + width && ym < y + height;
	minecraft->textures->loadAndBindTexture("gui/spritesheet.png");
	if (!active) glColor4f2(0.5f, 0.5f, 0.5f, 1);
	else if (down) glColor4f2(0.75f, 0.75f, 0.75f, 1);
	else glColor4f2(1, 1, 1, 1);
	// the 16x16 sprite has a 1px outline and a 2px bevel; scale the border with the button
	const int b = 4;
	blitNineSlice((float)x, (float)y, (float)width, (float)height, 0, 0, 16, 16, b, 128);
	glColor4f2(1, 1, 1, 1);
}

//
// Square button with the 0.9.0 tools (wrench + hammer) icon
//
ToolsButton::ToolsButton(int id)
:	super(id, "")
{
	width = height = 46;
}

void ToolsButton::render(Minecraft* minecraft, int xm, int ym) {
	if (!visible) return;
	renderBg(minecraft, xm, ym);
	if (active && _currentlyDown && xm >= x && ym >= y && xm < x + width && ym < y + height) glColor4f2(0.75f, 0.75f, 0.75f, 1);
	minecraft->textures->loadAndBindTexture("gui/touchgui.png");
	const float s = height * 0.66f;
	blit(x + (width - s) * 0.5f, y + (height - s) * 0.5f, 219, 0, s, s, 20, 20);
	glColor4f2(1, 1, 1, 1);
}

static std::string pickSplash() {
	time_t now = time(NULL);
	struct tm* lt = localtime(&now);
	if (lt) {
		if (lt->tm_mon == 11 && lt->tm_mday == 24) return "Merry X-mas!";
		if (lt->tm_mon == 0 && lt->tm_mday == 1) return "Happy new year!";
		if (lt->tm_mon == 9 && lt->tm_mday == 31) return "OOoooOOOoooo! Spooky!";
	}
	static bool seeded = false;
	if (!seeded) { srand((unsigned int)now ^ 0x5eed); seeded = true; }
	return SPLASHES[rand() % SPLASH_COUNT];
}

//
// Start menu screen, as in Pocket Edition 0.9.0: the logo with a splash,
// one wide Play button, and the tools button in the bottom right corner.
// (Join Game lives on the world list, like 0.9.0's LAN games.)
//
StartMenuScreen::StartMenuScreen()
:	bHost(    2, "Play"),
	bOptions( 4),
	bBuy(     5),
	logoX(0), logoY(0), logoW(0), logoH(0)
{
	splash = pickSplash();
}

StartMenuScreen::~StartMenuScreen()
{
}

void StartMenuScreen::init()
{
	buttons.push_back(&bHost);
	buttons.push_back(&bOptions);

	tabButtons.push_back(&bHost);
	tabButtons.push_back(&bOptions);

	#ifdef DEMO_MODE
		buttons.push_back(&bBuy);
		tabButtons.push_back(&bBuy);
	#endif

	copyright = "\xffMojang AB";
	version = "v0.9.0 alpha";
	#ifdef DEMO_MODE
		version += " (Demo)";
	#endif

	bHost.active = bOptions.active = false;
}

void StartMenuScreen::setupPositions() {
	// proportions measured from a 960x540 screenshot of 0.9.0
	logoW = width * 0.521f;
	logoH = logoW * 64.0f / 274.0f;
	logoX = (width - logoW) * 0.5f;
	logoY = height * 0.065f;

	bHost.width  = (int)(width * 0.31f);
	bHost.height = (int)(height * 0.163f);
	if (bHost.height < 24) bHost.height = 24;
	bHost.x = (width - bHost.width) / 2;
	bHost.y = (int)(height * 0.552f);

	int s = (int)(height * 0.171f);
	if (s < 26) s = 26;
	bOptions.width = bOptions.height = s;
	bOptions.x = width - s - (int)(width * 0.0083f + 1);
	bOptions.y = height - s - (int)(height * 0.0185f + 1);

	bBuy.y = height - bBuy.height - 3;
	bBuy.x = (width - bBuy.width) / 2;
}

void StartMenuScreen::tick() {
	_updateLicense();
}

void StartMenuScreen::buttonClicked(::Button* button) {

	if (button->id == bHost.id)
	{
		#if defined(DEMO_MODE) || defined(APPLE_DEMO_PROMOTION)
			minecraft->setScreen( new SimpleChooseLevelScreen("_DemoLevel") );
		#else
			minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
		#endif
	}
	if (button->id == bOptions.id)
	{
		minecraft->setScreen(new OptionsScreen());
	}
	if (button->id == bBuy.id)
	{
		minecraft->platform()->buyGame();
	}
}

bool StartMenuScreen::isInGameScreen() { return false; }

void StartMenuScreen::render( int xm, int ym, float a )
{
	renderBackground();

	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if defined(RPI)
	TextureId id = minecraft->textures->loadTexture("gui/pi_title.png");
#else
	TextureId id = minecraft->textures->loadTexture("gui/title.png");
#endif
	if (minecraft->textures->getTemporaryTextureData(id)) {
		minecraft->textures->bind(id);
		Tesselator& t = Tesselator::instance;
		glColor4f2(1, 1, 1, 1);
		t.begin();
			t.vertexUV(logoX,         logoY + logoH, blitOffset, 0, 1);
			t.vertexUV(logoX + logoW, logoY + logoH, blitOffset, 1, 1);
			t.vertexUV(logoX + logoW, logoY,         blitOffset, 1, 0);
			t.vertexUV(logoX,         logoY,         blitOffset, 0, 0);
		t.draw();
	}

	// splash: yellow, tilted, gently pulsing (as in Java / PE)
	{
		glPushMatrix2();
		glTranslatef2(width * 0.715f, logoY + logoH * 0.85f, 0);
		glRotatef2(-20.0f, 0, 0, 1);
		const float ms = (float)(getTimeMs() % 1000) / 1000.0f;
		float sc = 1.8f - std::fabs(std::sin(ms * 6.2831855f)) * 0.1f;
		sc = sc * 100.0f / (float)(font->width(splash) + 32);
		sc *= 1.5f;           // PE's font is wider than Java's: match 0.9.0's splash size
		if (sc > 1.8f) sc = 1.8f;
		sc *= logoW / 250.0f; // keep it in proportion with the logo
		glScalef2(sc, sc, sc);
		drawCenteredString(font, splash, 0, -8, 0xffffff00);
		glPopMatrix2();
	}

	drawString(font, version,   2, (int)(height * 0.889f), 0xffffffff);
	drawString(font, copyright, 2, (int)(height * 0.939f), 0xffffffff);

	glColor4f2(1, 1, 1, 1);
	Screen::render(xm, ym, a);
	glDisable2(GL_BLEND);
}

void StartMenuScreen::_updateLicense()
{
	int id = minecraft->getLicenseId();
	if (LicenseCodes::isReady(id))
	{
		if (LicenseCodes::isOk(id))
			bHost.active = bOptions.active = true;
		else
		{
			bool hasBuyButton = minecraft->platform()->hasBuyButtonWhenInvalidLicense();
			minecraft->setScreen(new InvalidLicenseScreen(id, hasBuyButton));
		}
	} else {
		bHost.active = bOptions.active = false;
	}
}

void StartMenuScreen::mouseClicked(int x, int y, int buttonNum) {
	Screen::mouseClicked(x, y, buttonNum);
}

bool StartMenuScreen::handleBackEvent( bool isDown ) {
	minecraft->quit();
	return true;
}

} // namespace Touch
