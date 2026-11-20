#include "TouchCreativeScreen.h"
#include "../../Gui.h"
#include "../../Font.h"
#include "../../../Minecraft.h"
#include "../../../player/LocalPlayer.h"
#include "../../../renderer/gles.h"
#include "../../../renderer/Textures.h"
#include "../../../renderer/entity/ItemRenderer.h"
#include "../../../sound/SoundEngine.h"
#include "../../../../world/entity/player/Inventory.h"
#include "../../../../world/item/Item.h"
#include "../../../../world/item/ItemCategory.h"
#include "../../../../world/level/tile/Tile.h"
#include "../../../../world/level/tile/Tiles09.h"
#include "../../../../world/item/SpawnEggItem.h"
#include <map>
#include <algorithm>
#include "../../../../platform/input/Mouse.h"
#include "../../../../platform/input/Keyboard.h"
#include "../../../../util/Mth.h"
#include <cmath>

namespace Touch {

// Colours of 0.9.0's GUI sheets (spritesheet.png)
static const int C_LIGHT     = 0xffbbb0aa;
static const int C_HIGHLIGHT = 0xffd4cdc8;
static const int C_MID       = 0xff958681;
static const int C_DARK      = 0xff534950;
static const int C_CONTENT   = 0xff403a40;
static const int C_CELL      = 0xff5d5258;
static const int C_CELL_DARK = 0xff1e171e;
static const int C_CELL_LIGHT= 0xff8f848c;

int CreativeScreen::lastTab = CreativeScreen::TAB_BLOCKS;

CreativeScreen::CreativeScreen()
:	tab(lastTab),
	panelX(0), panelY(0), panelW(0), panelH(0), unit(1),
	closeX(0), closeY(0), closeS(0),
	tabX(0), tabY0(0), tabW(0), tabH(0), tabPitch(0),
	contentX(0), contentY(0), contentW(0), contentH(0),
	gridX(0), gridY(0), pitch(1), columns(8),
	scroll(0), scrollVel(0),
	dragging(false), dragMoved(false), closePressed(false),
	downX(0), downY(0), downScroll(0), lastDragY(0), lastMouseY(0)
{
	tabIcons[TAB_ITEMS]      = ItemInstance(Item::seeds_wheat);
	tabIcons[TAB_TOOLS]      = ItemInstance(Item::sword_iron);
	tabIcons[TAB_DECORATION] = ItemInstance(Tile::bookshelf);
	tabIcons[TAB_BLOCKS]     = ItemInstance(Tile::redBrick);
}

CreativeScreen::~CreativeScreen()
{
}

// The order of the decoration and tools tabs in 0.9.0 (from screenshots),
// for the items this game has. key = id << 16 | data (data 0xffff = any).
static const std::map<int, int>& layout09()
{
	static std::map<int, int> m;
	if (!m.empty()) return m;
	struct Add {
		std::map<int, int>& m; int tab; int n;
		void operator()(int id, int data = 0xffff) { if (id > 0) { int k = (id << 16) | (data & 0xffff); if (!m.count(k)) m[k] = tab * 10000 + n++; } }
		void tile(Tile* t, int data = 0xffff) { if (t) (*this)(t->id, data); }
		void item(Item* i, int data = 0xffff) { if (i) (*this)(i->id, data); }
	};
	// decoration (bookshelf tab)
	Add d = { m, 2, 0 };
	d.tile(Tile::goldBlock); d.tile(Tile::ironBlock); d.tile(Tile::emeraldBlock); d.tile(Tile::lapisBlock);
	d.tile(Tile::coalBlock); d.tile(Tile::topSnow); d.tile(Tile::glass); d.tile(Tile::lightGem);
	d.tile(Tile::netherReactor); d.tile(Tile::ladder); d.tile(Tile::thinGlass);
	d.item(Item::door_wood); d.tile(Tile::trapdoor); d.tile(Tile::fence); d.tile(Tile::fenceGate);
	d.item(Item::bed); d.tile(Tile::bookshelf); d.item(Item::painting); d.tile(Tile::workBench);
	d.tile(Tile::stonecutterBench); d.tile(Tile::chest); d.tile(Tile::furnace);
	d.tile((Tile*)Tile::flower);
	for (int f = 0; f < FlowerTile::TYPE_COUNT; ++f) d.tile((Tile*)Tile::rose, f);
	d.tile(Tile::tallgrass, 1); d.tile(Tile::tallgrass, 2);
	d.tile((Tile*)Tile::mushroom1); d.tile((Tile*)Tile::mushroom2);
	d.tile(Tile::cactus); d.tile(Tile::melon);
	d.item(Item::reeds); d.tile(Tile::sapling); d.tile((Tile*)Tile::leaves); d.tile((Tile*)Tile::leaves2);
	d.item(Item::sign); d.tile(Tile::cloth);
	// tools (sword tab)
	Add t = { m, 1, 0 };
	t.tile(Tile::torch);
	t.tile(Tile::tnt); t.item(Item::pickAxe_iron); t.item(Item::sword_iron);
	t.item(Item::bow); t.item(Item::flintAndSteel); t.item((Item*)Item::shears); t.item(Item::clock);
	t.item(Item::compass);
	{ int n; const SpawnEggItem::Egg* e = SpawnEggItem::eggs(n); for (int i = 0; i < n; ++i) t.item(Item::spawnEgg, e[i].mob); }
	t.item(Item::hoe_iron);
	return m;
}

int CreativeScreen::tabFor(const ItemInstance* it, int* order)
{
	const int id = it->id;
	const std::map<int, int>& m = layout09();
	std::map<int, int>::const_iterator f = m.find((id << 16) | (it->getAuxValue() & 0xffff));
	if (f == m.end()) f = m.find((id << 16) | 0xffff);
	if (f != m.end()) { if (order) *order = f->second % 10000; return f->second / 10000; }
	if (order) *order = 9999;
	Item* item = (id >= 0 && id < 512) ? Item::items[id] : NULL;
	int cat = -1;
	if (id > 0 && id < 256 && Tile::tiles[id]) cat = Tile::tiles[id]->category;
	if (cat < 0 && item) cat = item->category;
	if (item && item->isArmor()) return TAB_TOOLS;
	switch (cat) {
	case ItemCategory::Structures:  return TAB_BLOCKS;
	case ItemCategory::Decorations: return TAB_DECORATION;
	case ItemCategory::Tools:
	case ItemCategory::Mechanisms:  return TAB_TOOLS;
	case ItemCategory::FoodArmor:   return TAB_ITEMS;
	}
	return id < 256 ? TAB_BLOCKS : TAB_ITEMS;
}

void CreativeScreen::buildTab()
{
	entries.clear();
	Inventory* inv = minecraft->player->inventory;
	std::vector<std::pair<int, int> > sorted; // (0.9.0 order, slot)
	for (int i = Inventory::MAX_SELECTION_SIZE; i < inv->getContainerSize(); ++i) {
		ItemInstance* it = inv->getItem(i);
		if (!it || it->isNull() || it->id <= 0) continue;
		int order = 9999;
		if (tabFor(it, &order) != tab) continue;
		sorted.push_back(std::make_pair(order * 1000 + (int)sorted.size(), i));
	}
	std::sort(sorted.begin(), sorted.end());
	for (unsigned int k = 0; k < sorted.size(); ++k) {
		Entry e; e.slot = sorted[k].second;
		entries.push_back(e);
	}
	scroll = 0;
	scrollVel = 0;
}

void CreativeScreen::init()
{
	buildTab();
}

void CreativeScreen::setupPositions()
{
	// proportions measured from 0.9.0 screenshots (960 px wide, cell pitch 97 px)
	const float hotbarH = 24;
	panelX = (float)(int)Mth::Max(2.0f, width * 0.019f);
	panelY = (float)(int)Mth::Max(2.0f, height * 0.017f);
	const float right = width - (float)(int)Mth::Max(2.0f, width * 0.008f);
	panelW = right - panelX;
	panelH = height - hotbarH - panelY - 1;

	unit = panelH / 6.5f;                       // X + four tabs fit the height
	if (unit > panelW / 9.63f) unit = panelW / 9.63f;

	closeS = unit * 0.67f;
	closeX = panelX + unit * 0.23f;
	closeY = panelY + unit * 0.27f;

	tabX = panelX;
	tabW = unit * 1.03f;
	tabH = unit * 1.13f;
	tabPitch = unit * 1.24f;
	tabY0 = panelY + unit * 1.53f;

	contentX = panelX + unit * 1.19f;
	contentY = panelY + unit * 0.165f;
	contentW = right - unit * 0.12f - contentX;
	contentH = panelY + panelH - unit * 0.12f - contentY;

	// 8 columns (0.9.0); on wide screens cells are capped so about four rows
	// show, and the extra width becomes extra columns
	columns = 8;
	pitch = contentW / 8.31f;
	if (pitch > contentH / 4.2f) {
		pitch = contentH / 4.2f;
		columns = (int)((contentW - pitch * 0.31f) / pitch);
		if (columns < 1) columns = 1;
	}
	gridX = contentX + (contentW - columns * pitch) * 0.5f;
	gridY = contentY + pitch * 0.155f;
	if (scroll > maxScroll()) scroll = maxScroll();
}

float CreativeScreen::maxScroll() const
{
	const int rows = ((int)entries.size() + columns - 1) / columns;
	const float total = rows * pitch + pitch * 0.31f;
	return total > contentH ? total - contentH : 0.0f;
}

int CreativeScreen::cellAt(int x, int y)
{
	if (!inside((float)x, (float)y, contentX, contentY, contentW, contentH)) return -1;
	const float gx = x - gridX, gy = y - gridY + scroll;
	if (gx < 0 || gy < 0) return -1;
	const int c = (int)(gx / pitch), r = (int)(gy / pitch);
	if (c >= columns) return -1;
	const int i = r * columns + c;
	return i < (int)entries.size() ? i : -1;
}

void CreativeScreen::pick(int entry)
{
	Inventory* inv = minecraft->player->inventory;
	const int slot = entries[entry].slot;
	if (!inv->getItem(slot)) return;
	inv->moveToSelectionSlot(0, slot, true);
	inv->selectSlot(0);
	minecraft->soundEngine->playUI("random.pop2", 1.0f, 0.3f);
	minecraft->gui.flashSlot(inv->selected);
	minecraft->gui.inventoryUpdated();
}

void CreativeScreen::mouseClicked(int x, int y, int buttonNum)
{
	if (inside((float)x, (float)y, closeX, closeY, closeS, closeS)) { closePressed = true; return; }
	for (int t = 0; t < NUM_TABS; ++t) {
		if (inside((float)x, (float)y, tabX, tabY0 + t * tabPitch, tabW, tabH)) {
			if (t != tab) {
				tab = lastTab = t;
				buildTab();
				minecraft->soundEngine->playUI("random.click", 1.0f, 1.0f);
			}
			return;
		}
	}
	if (inside((float)x, (float)y, contentX, contentY, contentW, contentH)) {
		dragging = true;
		dragMoved = false;
		downX = x; downY = y;
		downScroll = scroll;
		lastDragY = lastMouseY = y;
		scrollVel = 0;
	}
}

void CreativeScreen::mouseReleased(int x, int y, int buttonNum)
{
	if (closePressed) {
		closePressed = false;
		if (inside((float)x, (float)y, closeX, closeY, closeS, closeS)) minecraft->setScreen(NULL);
		return;
	}
	if (dragging) {
		dragging = false;
		if (!dragMoved) {
			const int i = cellAt(x, y);
			if (i >= 0) pick(i);
		}
	}
}

void CreativeScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE || eventKey == Keyboard::KEY_E) { minecraft->setScreen(NULL); return; }
	super::keyPressed(eventKey);
}

bool CreativeScreen::handleBackEvent(bool isDown)
{
	if (!isDown) minecraft->setScreen(NULL);
	return true;
}

void CreativeScreen::tick()
{
	if (dragging) {
		// fling speed: how far the finger moved since the last tick
		scrollVel = (float)(lastDragY - lastMouseY);
		lastDragY = lastMouseY;
	} else if (scrollVel != 0) {
		scroll += scrollVel;
		scrollVel *= 0.82f;
		if (std::fabs(scrollVel) < 0.3f) scrollVel = 0;
	}
	const float m = maxScroll();
	if (scroll < 0) { scroll = 0; scrollVel = 0; }
	if (scroll > m) { scroll = m; scrollVel = 0; }
}

void CreativeScreen::removed()
{
	minecraft->gui.inventoryUpdated();
}

void CreativeScreen::renderCell(float x, float y, float s)
{
	const float b = s > 30 ? 2.0f : 1.0f;
	fill(x, y, x + s, y + s, C_CELL);
	fill(x, y, x + s, y + b, C_CELL_DARK);           // inset: dark top / left
	fill(x, y, x + b, y + s, C_CELL_DARK);
	fill(x + b, y + s - b, x + s, y + s, C_CELL_LIGHT); // light bottom / right
	fill(x + s - b, y + b, x + s, y + s, C_CELL_LIGHT);
}

void CreativeScreen::renderItem(const ItemInstance* item, float cx, float cy, float size)
{
	if (!item || item->isNull()) return;
	ItemInstance copy(*item);
	// scale with the matrix: blocks without a pre-rendered icon only draw at 16x16
	glPushMatrix2();
	glTranslatef2(cx - size * 0.5f, cy - size * 0.5f, 0);
	const float k = size / 16.0f;
	glScalef2(k, k, 1);
	ItemRenderer::renderGuiItem(minecraft->font, minecraft->textures, &copy, 0, 0, 16, 16, true);
	glPopMatrix2();
}

void CreativeScreen::renderTab(int t, bool selected)
{
	const float y = tabY0 + t * tabPitch;
	if (selected) {
		// joins the panel and the content frame
		fill(tabX, y, contentX, y + tabH, C_LIGHT);
		fill(tabX, y, contentX, y + 1, C_HIGHLIGHT);
		fill(tabX, y, tabX + 1, y + tabH, C_HIGHLIGHT);
		fill(tabX, y + tabH - 1, contentX, y + tabH, C_DARK);
	} else {
		fill(tabX, y, tabX + tabW, y + tabH, C_MID);
		fill(tabX, y, tabX + tabW, y + 1, C_LIGHT);
		fill(tabX, y, tabX + 1, y + tabH, C_LIGHT);
		fill(tabX + tabW - 1, y, tabX + tabW, y + tabH, C_HIGHLIGHT);
		fill(tabX, y + tabH - 1, tabX + tabW, y + tabH, C_DARK);
	}
}

void CreativeScreen::renderPanel()
{
	// panel
	fill(panelX, panelY, panelX + panelW, panelY + panelH, C_LIGHT);
	fill(panelX, panelY, panelX + panelW, panelY + 1, C_HIGHLIGHT);
	fill(panelX, panelY, panelX + 1, panelY + panelH, C_HIGHLIGHT);
	fill(panelX, panelY + panelH - 1, panelX + panelW, panelY + panelH, C_DARK);
	fill(panelX + panelW - 1, panelY, panelX + panelW, panelY + panelH, C_DARK);
	// content frame
	fill(contentX, contentY, contentX + contentW, contentY + contentH, C_CONTENT);
	fill(contentX, contentY, contentX + contentW, contentY + 1, C_CELL_DARK);
	fill(contentX, contentY, contentX + 1, contentY + contentH, C_CELL_DARK);
	fill(contentX, contentY + contentH - 1, contentX + contentW, contentY + contentH, C_HIGHLIGHT);
	fill(contentX + contentW - 1, contentY, contentX + contentW, contentY + contentH, C_HIGHLIGHT);
	// tabs
	for (int t = 0; t < NUM_TABS; ++t) if (t != tab) renderTab(t, false);
	renderTab(tab, true);
}

void CreativeScreen::render(int xm, int ym, float a)
{
	lastMouseY = ym;
	if (dragging && Mouse::isButtonDown(MouseAction::ACTION_LEFT)) {
		if (std::abs(ym - downY) > 4 || std::abs(xm - downX) > 4) dragMoved = true;
		if (dragMoved) {
			scroll = downScroll - (ym - downY);
			const float m = maxScroll();
			if (scroll < 0) scroll = 0;
			if (scroll > m) scroll = m;
		}
	}

	glDisable2(GL_DEPTH_TEST);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	renderPanel();

	// close button (spritesheet: normal / pressed X)
	glColor4f2(1, 1, 1, 1);
	minecraft->textures->loadAndBindTexture("gui/spritesheet.png");
	const bool down = closePressed && inside((float)xm, (float)ym, closeX, closeY, closeS, closeS);
	blitNineSlice(closeX, closeY, closeS, closeS, down ? 78 : 60, 0, 18, 18, 0, 128);

	// grid, clipped to the content frame
	const float gs = Gui::GuiScale;
	glEnable2(GL_SCISSOR_TEST);
	glScissor((int)(contentX * gs), (int)(minecraft->height - (contentY + contentH) * gs),
	          (int)(contentW * gs), (int)(contentH * gs));
	const float cell = pitch * 0.93f;
	const int firstRow = (int)((scroll - pitch * 0.155f) / pitch) - 1;
	const int visRows = (int)(contentH / pitch) + 3;
	for (int r = Mth::Max(0, firstRow); r < firstRow + visRows; ++r) {
		for (int c = 0; c < columns; ++c) {
			const int i = r * columns + c;
			const float x = gridX + c * pitch, y = gridY + r * pitch - scroll;
			renderCell(x, y, cell);
			(void)i;
		}
	}
	Inventory* inv = minecraft->player->inventory;
	const float itemSize = pitch * 0.56f;
	for (int r = Mth::Max(0, firstRow); r < firstRow + visRows; ++r) {
		for (int c = 0; c < columns; ++c) {
			const int i = r * columns + c;
			if (i >= (int)entries.size()) break;
			const float x = gridX + c * pitch, y = gridY + r * pitch - scroll;
			renderItem(inv->getItem(entries[i].slot), x + cell * 0.5f, y + cell * 0.5f, itemSize);
		}
	}
	glDisable2(GL_SCISSOR_TEST);

	// scroll indicator at the right edge of the frame (0.9.0)
	const float m = maxScroll();
	if (m > 0) {
		const float trackY = contentY + 2, trackH = contentH - 4;
		const float barH = Mth::Max(8.0f, trackH * contentH / (contentH + m));
		const float barY = trackY + (trackH - barH) * (scroll / m);
		const float bx = contentX + contentW - 3;
		fill(bx, barY, bx + 2, barY + barH, C_HIGHLIGHT);
	}

	// tab icons on top
	for (int t = 0; t < NUM_TABS; ++t) {
		const float y = tabY0 + t * tabPitch;
		renderItem(&tabIcons[t], tabX + tabW * 0.5f, y + tabH * 0.5f, unit * 0.6f);
	}

	glColor4f2(1, 1, 1, 1);
	glDisable2(GL_BLEND);
	glEnable2(GL_DEPTH_TEST);
}

}
