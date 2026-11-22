#include "JavaCreativeScreen.h"
#include "touch/TouchCreativeScreen.h"
#include "../Font.h"
#include "../../Minecraft.h"
#include "../../player/LocalPlayer.h"
#include "../../renderer/Textures.h"
#include "../../renderer/gles.h"
#include "../../renderer/entity/ItemRenderer.h"
#include "../../sound/SoundEngine.h"
#include "../../../world/entity/player/Inventory.h"
#include "../../../world/item/Item.h"
#include "../../../world/level/tile/Tile.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/Mouse.h"
#include "../../../util/Mth.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

static const int PANEL_W = 195, PANEL_H = 136;
static const int GRID_X = 9, GRID_Y = 18, PITCH = 18;
static const int HOTBAR_Y = 112;
static const int SCROLL_X = 175, SCROLL_Y = 18, SCROLL_H = 112;
static const int TAB_W = 28, TAB_H = 32;

int JavaCreativeScreen::lastTab = 0;

static const char* tabName(int t) {
	switch (t) {
	case 0: return "Building Blocks";
	case 1: return "Decoration Blocks";
	case 2: return "Tools";
	case 3: return "Miscellaneous";
	default: return "Saved Hotbars";
	}
}

JavaCreativeScreen::JavaCreativeScreen()
:	tab(lastTab), scroll(0), draggingScroller(false), heldSlot(-1), ui(1), left(0), top(0)
{
	passEvents = false;
	tabIcons[TAB_BLOCKS]     = ItemInstance(Tile::redBrick);
	tabIcons[TAB_DECORATION] = ItemInstance((Tile*)Tile::rose);
	tabIcons[TAB_TOOLS]      = ItemInstance(Item::pickAxe_iron);
	tabIcons[TAB_MISC]       = ItemInstance(Item::stick);
	tabIcons[TAB_SAVED]      = ItemInstance(Tile::bookshelf);
}

void JavaCreativeScreen::init()
{
	loadHotbars();
	buildList();
}

void JavaCreativeScreen::setupPositions()
{
	// panel plus a tab above and below
	const float needW = PANEL_W, needH = PANEL_H + 2 * (TAB_H - 4);
	ui = Mth::Min(1.0f, Mth::Min((width - 4) / needW, (height - 4) / needH));
	left = (width - PANEL_W * ui) * 0.5f;
	top = (height - PANEL_H * ui) * 0.5f;
}

void JavaCreativeScreen::buildList()
{
	list.clear();
	scroll = 0;
	if (tab == TAB_SAVED) return;
	static const int map[4] = { Touch::CreativeScreen::TAB_BLOCKS, Touch::CreativeScreen::TAB_DECORATION,
	                            Touch::CreativeScreen::TAB_TOOLS, Touch::CreativeScreen::TAB_ITEMS };
	Inventory* inv = minecraft->player->inventory;
	std::vector<std::pair<int, int> > sorted;
	for (int i = Inventory::MAX_SELECTION_SIZE; i < inv->getContainerSize(); ++i) {
		ItemInstance* it = inv->getItem(i);
		if (!it || it->isNull() || it->id <= 0) continue;
		int order = 9999;
		if (Touch::CreativeScreen::tabFor(it, &order) != map[tab]) continue;
		sorted.push_back(std::make_pair(order * 1000 + (int)sorted.size(), i));
	}
	std::sort(sorted.begin(), sorted.end());
	for (unsigned int k = 0; k < sorted.size(); ++k) list.push_back(sorted[k].second);
}

int JavaCreativeScreen::rows() const
{
	return ((int)list.size() + 8) / 9;
}

void JavaCreativeScreen::toGui(int sx, int sy, float& gx, float& gy) const
{
	gx = (sx - left) / ui;
	gy = (sy - top) / ui;
}

int JavaCreativeScreen::gridAt(float gx, float gy) const
{
	if (gx < GRID_X || gy < GRID_Y || gx >= GRID_X + 9 * PITCH || gy >= GRID_Y + 5 * PITCH) return -1;
	const int c = (int)(gx - GRID_X) / PITCH, r = (int)(gy - GRID_Y) / PITCH;
	if (tab == TAB_SAVED) return r * 9 + c; // saved row r, slot c
	const int i = (r + scroll) * 9 + c;
	return i < (int)list.size() ? i : -1;
}

int JavaCreativeScreen::hotbarAt(float gx, float gy) const
{
	if (gy < HOTBAR_Y || gy >= HOTBAR_Y + PITCH || gx < GRID_X || gx >= GRID_X + 9 * PITCH) return -1;
	return (int)(gx - GRID_X) / PITCH;
}

bool JavaCreativeScreen::tabRect(int t, float& x, float& y) const
{
	if (t < TAB_SAVED) { x = (float)(t * (TAB_W + 1)); y = (float)(-TAB_H + 4); }
	else { x = 0; y = (float)(PANEL_H - 4); }
	return true;
}

int JavaCreativeScreen::tabAt(float gx, float gy) const
{
	for (int t = 0; t < NUM_TABS; ++t) {
		float x, y;
		tabRect(t, x, y);
		if (gx >= x && gx < x + TAB_W && gy >= y && gy < y + TAB_H) return t;
	}
	return -1;
}

const ItemInstance* JavaCreativeScreen::listItem(int i)
{
	if (i < 0 || i >= (int)list.size()) return NULL;
	return minecraft->player->inventory->getItem(list[i]);
}

int JavaCreativeScreen::creativeSlotFor(int id, int aux)
{
	Inventory* inv = minecraft->player->inventory;
	for (int i = Inventory::MAX_SELECTION_SIZE; i < inv->getContainerSize(); ++i) {
		ItemInstance* it = inv->getItem(i);
		if (it && !it->isNull() && it->id == id && it->getAuxValue() == aux) return i;
	}
	return -1;
}

void JavaCreativeScreen::linkHotbar(int h, int creativeSlot)
{
	Inventory* inv = minecraft->player->inventory;
	if (h < 0 || h >= 9) return;
	if (creativeSlot < 0) inv->linkedSlots[h].inventorySlot = -1;
	else inv->linkSlot(h, creativeSlot, false);
	minecraft->gui.inventoryUpdated();
}

// ---- saved hotbars ("id.aux,id.aux,...;...") ----
void JavaCreativeScreen::loadHotbars()
{
	saved.assign(9, Hotbar());
	for (int r = 0; r < 9; ++r) for (int s = 0; s < 9; ++s) { saved[r].id[s] = 0; saved[r].aux[s] = 0; }
	std::stringstream rowsIn(minecraft->options.savedHotbars);
	std::string row;
	for (int r = 0; r < 9 && std::getline(rowsIn, row, ';'); ++r) {
		std::stringstream slots(row);
		std::string slot;
		for (int s = 0; s < 9 && std::getline(slots, slot, ','); ++s) {
			saved[r].id[s] = std::atoi(slot.c_str());
			const size_t dot = slot.find('.');
			saved[r].aux[s] = dot != std::string::npos ? std::atoi(slot.c_str() + dot + 1) : 0;
		}
	}
}

void JavaCreativeScreen::storeHotbars()
{
	std::string out;
	for (int r = 0; r < 9; ++r) {
		if (r) out += ';';
		for (int s = 0; s < 9; ++s) {
			char buf[24];
			sprintf(buf, "%s%d.%d", s ? "," : "", saved[r].id[s], saved[r].aux[s]);
			out += buf;
		}
	}
	minecraft->options.savedHotbars = out;
	minecraft->options.save();
}

void JavaCreativeScreen::saveHotbar(int row)
{
	Inventory* inv = minecraft->player->inventory;
	for (int s = 0; s < 9; ++s) {
		ItemInstance* it = inv->getItem(s); // linked hotbar slot
		saved[row].id[s] = (it && !it->isNull()) ? it->id : 0;
		saved[row].aux[s] = (it && !it->isNull()) ? it->getAuxValue() : 0;
	}
	storeHotbars();
	char buf[48];
	sprintf(buf, "Saved hotbar %d", row + 1);
	minecraft->gui.addMessage(buf);
}

void JavaCreativeScreen::restoreHotbar(int row)
{
	for (int s = 0; s < 9; ++s)
		linkHotbar(s, saved[row].id[s] > 0 ? creativeSlotFor(saved[row].id[s], saved[row].aux[s]) : -1);
	minecraft->soundEngine->playUI("random.pop2", 1.0f, 0.3f);
}

// ---- input ----
void JavaCreativeScreen::mouseClicked(int x, int y, int buttonNum)
{
	float gx, gy;
	toGui(x, y, gx, gy);
	const bool shift = Keyboard::isKeyDown(Keyboard::KEY_LSHIFT);

	const int t = tabAt(gx, gy);
	if (t >= 0) {
		if (t != tab) { tab = lastTab = t; buildList(); minecraft->soundEngine->playUI("random.click", 1.0f, 1.0f); }
		return;
	}
	if (tab != TAB_SAVED && gx >= SCROLL_X && gx < SCROLL_X + 12 && gy >= SCROLL_Y && gy < SCROLL_Y + SCROLL_H) {
		draggingScroller = maxScroll() > 0;
		return;
	}
	const int h = hotbarAt(gx, gy);
	if (h >= 0) {
		Inventory* inv = minecraft->player->inventory;
		const int was = inv->linkedSlots[h].inventorySlot;
		if (heldSlot >= 0) {
			linkHotbar(h, heldSlot);
			heldSlot = (buttonNum == MouseAction::ACTION_RIGHT) ? heldSlot : (was >= Inventory::MAX_SELECTION_SIZE ? was : -1);
		} else if (shift) {
			linkHotbar(h, -1);
		} else if (was >= Inventory::MAX_SELECTION_SIZE && inv->getItem(h)) {
			heldSlot = was;
			linkHotbar(h, -1);
		}
		return;
	}
	const int g = gridAt(gx, gy);
	if (g >= 0) {
		if (tab == TAB_SAVED) { restoreHotbar(g / 9); return; }
		if (heldSlot >= 0) { heldSlot = -1; return; } // put it back
		if (shift) {
			// first empty hotbar slot, else the selected one
			Inventory* inv = minecraft->player->inventory;
			int target = inv->selected;
			for (int s = 0; s < 9; ++s) if (!inv->getItem(s)) { target = s; break; }
			linkHotbar(target, list[g]);
			return;
		}
		heldSlot = list[g];
		return;
	}
	// clicked outside: drop what's on the cursor (creative: it just goes away)
	if (gx < 0 || gy < 0 || gx >= PANEL_W || gy >= PANEL_H) heldSlot = -1;
}

void JavaCreativeScreen::mouseReleased(int x, int y, int buttonNum)
{
	draggingScroller = false;
}

void JavaCreativeScreen::mouseEvent()
{
	const MouseAction& e = Mouse::getEvent();
	if (e.action == MouseAction::ACTION_WHEEL) {
		if (tab == TAB_SAVED) return;
		scroll -= e.dy > 0 ? 1 : (e.dy < 0 ? -1 : 0);
		if (scroll < 0) scroll = 0;
		if (scroll > maxScroll()) scroll = maxScroll();
		return;
	}
	super::mouseEvent();
}

void JavaCreativeScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE || eventKey == Keyboard::KEY_E) { minecraft->setScreen(NULL); return; }
	if (eventKey >= '1' && eventKey <= '9') {
		const int n = eventKey - '1';
		if (Keyboard::isKeyDown(Keyboard::KEY_C)) { saveHotbar(n); return; }
		if (Keyboard::isKeyDown(Keyboard::KEY_X)) { restoreHotbar(n); return; }
		// hovering an item: put it in that hotbar slot (Java)
		int mx = Mouse::getX(), my = Mouse::getY();
		toGUICoordinate(mx, my);
		float gx, gy;
		toGui(mx, my, gx, gy);
		const int g = gridAt(gx, gy);
		if (g >= 0 && tab != TAB_SAVED) linkHotbar(n, list[g]);
		return;
	}
	super::keyPressed(eventKey);
}

bool JavaCreativeScreen::handleBackEvent(bool isDown)
{
	if (!isDown) minecraft->setScreen(NULL);
	return true;
}

void JavaCreativeScreen::removed()
{
	minecraft->gui.inventoryUpdated();
}

// ---- drawing ----
void JavaCreativeScreen::drawItem(const ItemInstance* item, float x, float y)
{
	if (!item || item->isNull()) return;
	ItemInstance copy(*item);
	ItemRenderer::renderGuiItem(minecraft->font, minecraft->textures, &copy, x, y, true);
}

void JavaCreativeScreen::drawTooltip(const std::string& name, int xm, int ym)
{
	const int w = minecraft->font->width(name);
	int tx = xm + 8, ty = ym - 12;
	if (tx + w + 6 > width) tx = width - w - 6;
	if (ty < 2) ty = 2;
	fill(tx - 3, ty - 3, tx + w + 3, ty + 11, 0xf0100010);
	fill(tx - 3, ty - 3, tx + w + 3, ty - 2, 0x505000ff);
	fill(tx - 3, ty + 10, tx + w + 3, ty + 11, 0x505000ff);
	minecraft->font->drawShadow(name, (float)tx, (float)ty, 0xffffffff);
}

void JavaCreativeScreen::render(int xm, int ym, float a)
{
	float gx, gy;
	toGui(xm, ym, gx, gy);
	if (draggingScroller && Mouse::isButtonDown(MouseAction::ACTION_LEFT) && maxScroll() > 0) {
		const float f = (gy - SCROLL_Y - 7.5f) / (SCROLL_H - 15.0f);
		scroll = (int)(Mth::clamp(f, 0.0f, 1.0f) * maxScroll() + 0.5f);
	}

	fillGradient(0, 0, width, height, 0xc0101010, 0xd0101010);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable2(GL_DEPTH_TEST);

	glPushMatrix();
	glTranslatef(left, top, 0);
	glScalef(ui, ui, 1);
	glColor4f2(1, 1, 1, 1);

	// unselected tabs behind the panel, the selected one in front
	minecraft->textures->loadAndBindTexture("gui/java_creative_tabs.png");
	for (int t = 0; t < NUM_TABS; ++t) {
		if (t == tab) continue;
		float x, y;
		tabRect(t, x, y);
		const bool bottom = t == TAB_SAVED;
		blit(x, y, (t % 6) * TAB_W, bottom ? 64 : 0, (float)TAB_W, (float)TAB_H, TAB_W, TAB_H);
	}
	minecraft->textures->loadAndBindTexture("gui/java_creative.png");
	blit(0, 0, 0, 0, PANEL_W, PANEL_H);
	{
		float x, y;
		tabRect(tab, x, y);
		const bool bottom = tab == TAB_SAVED;
		minecraft->textures->loadAndBindTexture("gui/java_creative_tabs.png");
		blit(x, y, (tab % 6) * TAB_W, bottom ? 96 : 32, (float)TAB_W, (float)TAB_H, TAB_W, TAB_H);
	}
	// scroller
	if (tab != TAB_SAVED) {
		minecraft->textures->loadAndBindTexture("gui/java_creative_tabs.png");
		const int m = maxScroll();
		const float sy = SCROLL_Y + (m > 0 ? (SCROLL_H - 15) * (float)scroll / m : 0);
		blit((float)SCROLL_X, sy, m > 0 ? 232 : 244, 0, 12.0f, 15.0f, 12, 15);
	}

	minecraft->font->draw(tabName(tab), 8.0f, 6.0f, 0xff404040, false);

	// tab icons
	for (int t = 0; t < NUM_TABS; ++t) {
		float x, y;
		tabRect(t, x, y);
		drawItem(&tabIcons[t], x + 6, y + (t == TAB_SAVED ? 9 : 9));
	}

	// grid
	int hoveredList = -1;
	const int g = gridAt(gx, gy);
	if (tab == TAB_SAVED) {
		for (int r = 0; r < 5; ++r)
			for (int s = 0; s < 9; ++s) {
				const Hotbar& hb = saved[r];
				if (hb.id[s] <= 0) continue;
				ItemInstance it(hb.id[s], 1, hb.aux[s]);
				if (Item::items[hb.id[s]]) drawItem(&it, (float)(GRID_X + s * PITCH), (float)(GRID_Y + r * PITCH));
			}
	} else {
		for (int r = 0; r < 5; ++r)
			for (int c = 0; c < 9; ++c)
				drawItem(listItem((r + scroll) * 9 + c), (float)(GRID_X + c * PITCH), (float)(GRID_Y + r * PITCH));
		hoveredList = g;
	}
	// hotbar
	Inventory* inv = minecraft->player->inventory;
	for (int s = 0; s < 9; ++s) drawItem(inv->getItem(s), (float)(GRID_X + s * PITCH), (float)HOTBAR_Y);

	// highlight
	const int h = hotbarAt(gx, gy);
	if (g >= 0) {
		const int c = g % 9, r = g / 9 - (tab == TAB_SAVED ? 0 : scroll);
		if (tab == TAB_SAVED) fill((float)GRID_X, (float)(GRID_Y + r * PITCH), (float)(GRID_X + 9 * PITCH - 2), (float)(GRID_Y + r * PITCH + 16), 0x80ffffff);
		else fill((float)(GRID_X + c * PITCH), (float)(GRID_Y + r * PITCH), (float)(GRID_X + c * PITCH + 16), (float)(GRID_Y + r * PITCH + 16), 0x80ffffff);
	}
	if (h >= 0) fill((float)(GRID_X + h * PITCH), (float)HOTBAR_Y, (float)(GRID_X + h * PITCH + 16), (float)(HOTBAR_Y + 16), 0x80ffffff);
	glPopMatrix();

	// cursor stack
	if (heldSlot >= 0) {
		glPushMatrix();
		glTranslatef((float)xm - 8 * ui, (float)ym - 8 * ui, 0);
		glScalef(ui, ui, 1);
		drawItem(inv->getItem(heldSlot), 0, 0);
		glPopMatrix();
	} else {
		// tooltips
		const int t = tabAt(gx, gy);
		if (t >= 0) drawTooltip(tabName(t), xm, ym);
		else if (hoveredList >= 0 && listItem(hoveredList)) drawTooltip(listItem(hoveredList)->getName(), xm, ym);
		else if (h >= 0 && inv->getItem(h)) drawTooltip(inv->getItem(h)->getName(), xm, ym);
		else if (tab == TAB_SAVED && g >= 0) {
			char buf[64];
			sprintf(buf, "Saved hotbar %d (click to load, C+%d to save)", g / 9 + 1, g / 9 + 1);
			drawTooltip(buf, xm, ym);
		}
	}
	glEnable2(GL_DEPTH_TEST);
	glDisable2(GL_BLEND);
}
