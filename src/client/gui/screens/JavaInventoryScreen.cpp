#include "JavaInventoryScreen.h"
#include "../../Minecraft.h"
#include "../Font.h"
#include "../../renderer/Textures.h"
#include "../../renderer/gles.h"
#include "../../renderer/entity/ItemRenderer.h"
#include "../../renderer/entity/EntityRenderDispatcher.h"
#include "../../player/LocalPlayer.h"
#include "../../../world/entity/player/Inventory.h"
#include "../../../world/inventory/CraftingContainer.h"
#include "../../../world/item/Item.h"
#include "../../../world/item/ArmorItem.h"
#include "../../../world/item/crafting/Recipes.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/Mouse.h"
#include "../../../network/eagler/EaglerClient.h"
#include "../../../util/Mth.h"
#include "../../../SharedConstants.h"
#include <cmath>
#include <cstdio>

static const int PANEL_W = 176, PANEL_H = 166;
static const int STORAGE_FIRST = 9;   // Inventory: slots 9..35 storage, 36..44 hotbar
static const int HOTBAR_FIRST = 36;

JavaInventoryScreen* JavaInventoryScreen::active = NULL;

void JavaInventoryScreen::fixHotbarLinks(Inventory* inv)
{
	for (int h = 0; h < 9; ++h)
		if (inv->linkedSlots[h].inventorySlot != HOTBAR_FIRST + h)
			inv->linkedSlots[h].inventorySlot = HOTBAR_FIRST + h;
}

JavaInventoryScreen::JavaInventoryScreen(bool table, int windowId)
:	table(table),
	windowId(windowId),
	gridW(table ? 3 : 2),
	craftSlots(new CraftingContainer(table ? 3 : 2, table ? 3 : 2)),
	ui(1), left(0), top(0),
	hovered(-1),
	lastMouseX(0), lastMouseY(0)
{
	passEvents = false;
}

JavaInventoryScreen::~JavaInventoryScreen()
{
	if (active == this) active = NULL;
	delete craftSlots;
}

void JavaInventoryScreen::init()
{
	active = this;
	fixHotbarLinks(minecraft->player->inventory);
	slots.clear();
	Slot s;
	const int base = table ? 1 : 1;                 // grid starts at window slot 1
	// result + grid
	if (table) {
		s.kind = RESULT; s.index = 0; s.windowSlot = 0; s.x = 124; s.y = 35; slots.push_back(s);
		for (int i = 0; i < 9; ++i) { s.kind = GRID; s.index = i; s.windowSlot = base + i; s.x = 30 + (i % 3) * 18; s.y = 17 + (i / 3) * 18; slots.push_back(s); }
	} else {
		s.kind = RESULT; s.index = 0; s.windowSlot = 0; s.x = 154; s.y = 28; slots.push_back(s);
		for (int i = 0; i < 4; ++i) { s.kind = GRID; s.index = i; s.windowSlot = base + i; s.x = 98 + (i % 2) * 18; s.y = 18 + (i / 2) * 18; slots.push_back(s); }
		for (int i = 0; i < 4; ++i) { s.kind = ARMOR; s.index = i; s.windowSlot = 5 + i; s.x = 8; s.y = 8 + i * 18; slots.push_back(s); }
	}
	// storage + hotbar (window numbering continues after grid / armour)
	const int firstStorageWindow = table ? 10 : 9;
	for (int i = 0; i < 27; ++i) { s.kind = STORAGE; s.index = STORAGE_FIRST + i; s.windowSlot = firstStorageWindow + i; s.x = 8 + (i % 9) * 18; s.y = 84 + (i / 9) * 18; slots.push_back(s); }
	for (int i = 0; i < 9; ++i) { s.kind = STORAGE; s.index = HOTBAR_FIRST + i; s.windowSlot = firstStorageWindow + 27 + i; s.x = 8 + i * 18; s.y = 142; slots.push_back(s); }
	setupPositions();
}

void JavaInventoryScreen::setupPositions()
{
	// scale the 176x166 panel to fit small screens (phones in landscape)
	ui = Mth::Min(1.0f, Mth::Min((width - 4) / (float)PANEL_W, (height - 4) / (float)PANEL_H));
	left = (width - PANEL_W * ui) * 0.5f;
	top = (height - PANEL_H * ui) * 0.5f;
}

void JavaInventoryScreen::toGui(int sx, int sy, float& gx, float& gy)
{
	gx = (sx - left) / ui;
	gy = (sy - top) / ui;
}

int JavaInventoryScreen::slotAt(int x, int y)
{
	float gx, gy;
	toGui(x, y, gx, gy);
	for (unsigned int i = 0; i < slots.size(); ++i) {
		const Slot& s = slots[i];
		const int pad = s.kind == RESULT && !table ? 0 : 0;
		if (gx >= s.x - 1 - pad && gx < s.x + 17 + pad && gy >= s.y - 1 - pad && gy < s.y + 17 + pad) return (int)i;
	}
	return -1;
}

int JavaInventoryScreen::maxStack(const ItemInstance& i)
{
	if (i.isNull() || i.id <= 0 || !Item::items[i.id]) return 64;
	return Item::items[i.id]->getMaxStackSize();
}

bool JavaInventoryScreen::same(const ItemInstance& a, const ItemInstance& b)
{
	return !a.isNull() && !b.isNull() && a.id == b.id && a.getAuxValue() == b.getAuxValue();
}

ItemInstance JavaInventoryScreen::get(const Slot& s)
{
	switch (s.kind) {
	case STORAGE: { ItemInstance* i = minecraft->player->inventory->getItem(s.index); return (i && !i->isNull() && i->count > 0) ? *i : ItemInstance(); }
	case GRID:    { ItemInstance* i = craftSlots->getItem(s.index); return (i && !i->isNull() && i->count > 0) ? *i : ItemInstance(); }
	case RESULT:  return resultItem;
	case ARMOR:   { ItemInstance* i = minecraft->player->getArmor(s.index); return (i && !i->isNull()) ? *i : ItemInstance(); }
	}
	return ItemInstance();
}

void JavaInventoryScreen::set(const Slot& s, const ItemInstance& item)
{
	const bool empty = item.isNull() || item.count <= 0;
	switch (s.kind) {
	case STORAGE: {
		Inventory* inv = minecraft->player->inventory;
		if (empty) inv->clearSlot(s.index);
		else { ItemInstance copy(item); inv->replaceSlot(s.index, &copy); }
		fixHotbarLinks(inv);
		break;
	}
	case GRID:
		craftSlots->setItem(s.index, empty ? ItemInstance() : item);
		if (!minecraft->eagler) updateResult();
		break;
	case RESULT:
		resultItem = empty ? ItemInstance() : item;
		break;
	case ARMOR:
		minecraft->player->setArmor(s.index, empty ? NULL : &item);
		break;
	}
}

void JavaInventoryScreen::updateResult()
{
	resultItem = Recipes::getInstance()->getItemFor(craftSlots);
	if (resultItem.count <= 0) resultItem = ItemInstance();
}

bool JavaInventoryScreen::addToRange(ItemInstance& item, int from, int to)
{
	Inventory* inv = minecraft->player->inventory;
	const int step = from <= to ? 1 : -1;
	// merge into matching stacks
	for (int i = from; item.count > 0; i += step) {
		ItemInstance* cur = inv->getItem(i);
		if (cur && same(*cur, item) && cur->count < maxStack(*cur)) {
			const int n = Mth::Min(item.count, maxStack(*cur) - cur->count);
			ItemInstance merged(*cur);
			merged.count += n;
			inv->replaceSlot(i, &merged);
			item.count -= n;
		}
		if (i == to) break;
	}
	// then empty slots
	for (int i = from; item.count > 0; i += step) {
		ItemInstance* cur = inv->getItem(i);
		if (!cur || cur->isNull() || cur->count <= 0) {
			ItemInstance copy(item);
			inv->replaceSlot(i, &copy);
			item.count = 0;
		}
		if (i == to) break;
	}
	fixHotbarLinks(inv);
	return item.count <= 0;
}

void JavaInventoryScreen::sendClick(int windowSlot, int button, int mode, const ItemInstance& clicked)
{
	if (minecraft->eagler) minecraft->eagler->sendWindowClick(windowId, windowSlot, button, mode, clicked);
}

void JavaInventoryScreen::craft(bool toInventory)
{
	if (resultItem.isNull()) return;
	ItemInstance out = resultItem;
	if (!toInventory) {
		if (!carried.isNull() && (!same(carried, out) || carried.count + out.count > maxStack(out))) return;
	}
	if (toInventory) {
		ItemInstance tmp = out;
		if (!addToRange(tmp, HOTBAR_FIRST + 8, STORAGE_FIRST)) {
			if (tmp.count == out.count) return; // nowhere to put it
			minecraft->player->drop(new ItemInstance(tmp), false);
		}
	} else {
		if (carried.isNull()) carried = out; else carried.count += out.count;
	}
	// use up one of each ingredient (containers like buckets stay behind)
	for (int i = 0; i < craftSlots->getContainerSize(); ++i) {
		ItemInstance* in = craftSlots->getItem(i);
		if (!in || in->isNull() || in->count <= 0) continue;
		ItemInstance left(*in);
		left.count -= 1;
		if (left.count <= 0) {
			Item* it = Item::items[in->id];
			if (it && it->hasCraftingRemainingItem()) left = ItemInstance(it->getCraftingRemainingItem());
			else left = ItemInstance();
		}
		craftSlots->setItem(i, left.count > 0 ? left : ItemInstance());
	}
	updateResult();
}

void JavaInventoryScreen::quickMove(int index)
{
	Slot& s = slots[index];
	ItemInstance item = get(s);
	if (item.isNull()) return;
	if (s.kind == RESULT) {
		// shift-click the result: craft as many as fit
		for (int guard = 0; guard < 64 && !resultItem.isNull() && same(resultItem, item); ++guard) {
			int before = resultItem.count;
			craft(true);
			if (before == 0) break;
		}
		return;
	}
	if (s.kind == STORAGE) {
		// armour from storage goes onto the body first
		if (!table && Item::items[item.id] && Item::items[item.id]->isArmor()) {
			const int a = ((ArmorItem*)Item::items[item.id])->slot;
			ItemInstance* worn = minecraft->player->getArmor(a);
			if (!worn || worn->isNull()) {
				ItemInstance one(item); one.count = 1;
				minecraft->player->setArmor(a, &one);
				item.count -= 1;
				set(s, item);
				return;
			}
		}
		const bool inHotbar = s.index >= HOTBAR_FIRST;
		ItemInstance moving = item;
		if (inHotbar) addToRange(moving, STORAGE_FIRST, HOTBAR_FIRST - 1);
		else addToRange(moving, HOTBAR_FIRST, HOTBAR_FIRST + 8);
		set(s, moving.count > 0 ? moving : ItemInstance());
		return;
	}
	// grid / armour -> inventory
	ItemInstance moving = item;
	addToRange(moving, STORAGE_FIRST, HOTBAR_FIRST + 8);
	set(s, moving.count > 0 ? moving : ItemInstance());
}

void JavaInventoryScreen::click(int index, int button, bool shift)
{
	Slot& s = slots[index];
	ItemInstance inSlot = get(s);
	sendClick(s.windowSlot, button, shift ? 1 : 0, inSlot);

	if (shift) { quickMove(index); return; }

	if (s.kind == RESULT) {
		if (minecraft->eagler) {
			// the server crafts; predict the cursor so it feels instant
			if (!resultItem.isNull() && (carried.isNull() || (same(carried, resultItem) && carried.count + resultItem.count <= maxStack(resultItem))))
				carried.isNull() ? (void)(carried = resultItem) : (void)(carried.count += resultItem.count);
			return;
		}
		craft(false);
		return;
	}
	if (s.kind == ARMOR && !carried.isNull()) {
		Item* it = Item::items[carried.id];
		if (!it || !it->isArmor() || ((ArmorItem*)it)->slot != s.index) return; // only matching armour
	}

	if (button == 0) {
		if (carried.isNull()) {                       // pick up the stack
			carried = inSlot;
			set(s, ItemInstance());
		} else if (inSlot.isNull()) {                 // put the stack down
			const int n = s.kind == ARMOR ? 1 : carried.count;
			ItemInstance put(carried); put.count = n;
			set(s, put);
			carried.count -= n;
			if (carried.count <= 0) carried = ItemInstance();
		} else if (same(inSlot, carried)) {           // merge
			const int n = Mth::Min(carried.count, maxStack(inSlot) - inSlot.count);
			inSlot.count += n; carried.count -= n;
			set(s, inSlot);
			if (carried.count <= 0) carried = ItemInstance();
		} else {                                      // swap
			set(s, carried);
			carried = inSlot;
		}
	} else {
		if (carried.isNull()) {                       // take half
			if (inSlot.isNull()) return;
			const int take = (inSlot.count + 1) / 2;
			carried = inSlot; carried.count = take;
			inSlot.count -= take;
			set(s, inSlot.count > 0 ? inSlot : ItemInstance());
		} else if (inSlot.isNull() || (same(inSlot, carried) && inSlot.count < maxStack(inSlot))) {
			if (inSlot.isNull()) { inSlot = carried; inSlot.count = 1; }  // place one
			else inSlot.count += 1;
			set(s, inSlot);
			carried.count -= 1;
			if (carried.count <= 0) carried = ItemInstance();
		} else {
			set(s, carried);
			carried = inSlot;
		}
	}
}

void JavaInventoryScreen::mouseClicked(int x, int y, int buttonNum)
{
	const int i = slotAt(x, y);
	const int button = buttonNum == MouseAction::ACTION_RIGHT ? 1 : 0;
	if (i >= 0) {
		click(i, button, Keyboard::isKeyDown(Keyboard::KEY_LSHIFT));
		return;
	}
	float gx, gy;
	toGui(x, y, gx, gy);
	const bool outside = gx < 0 || gy < 0 || gx >= PANEL_W || gy >= PANEL_H;
	if (outside && !carried.isNull()) {
		// click outside the window: throw the carried item (right click: one)
		sendClick(-999, button, 0, ItemInstance());
		ItemInstance drop(carried);
		if (button == 1) { drop.count = 1; carried.count -= 1; } else carried.count = 0;
		if (carried.count <= 0) carried = ItemInstance();
		if (!minecraft->eagler) minecraft->player->drop(new ItemInstance(drop), false);
	}
}

void JavaInventoryScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE || eventKey == Keyboard::KEY_E) {
		minecraft->setScreen(NULL);
		return;
	}
	// 1-9 over a slot: swap with that hotbar slot (Java)
	if (eventKey >= '1' && eventKey <= '9' && hovered >= 0) {
		Slot& s = slots[hovered];
		const int h = eventKey - '1';
		if (s.kind == RESULT) return;
		Slot hs; hs.kind = STORAGE; hs.index = HOTBAR_FIRST + h; hs.windowSlot = 0;
		if (s.kind == STORAGE && s.index == hs.index) return;
		sendClick(s.windowSlot, h, 2, get(s));
		ItemInstance a = get(s), b = get(hs);
		set(s, b); set(hs, a);
	}
}

bool JavaInventoryScreen::handleBackEvent(bool isDown)
{
	if (!isDown) minecraft->setScreen(NULL);
	return true;
}

void JavaInventoryScreen::tick()
{
	if (minecraft->player) fixHotbarLinks(minecraft->player->inventory);
}

void JavaInventoryScreen::removed()
{
	if (active == this) active = NULL;
	if (minecraft->eagler) {
		minecraft->eagler->sendCloseWindow(windowId);
	} else if (minecraft->player) {
		// crafting grid and cursor go back to the inventory (or on the ground)
		for (int i = 0; i < craftSlots->getContainerSize(); ++i) {
			ItemInstance* in = craftSlots->getItem(i);
			if (!in || in->isNull() || in->count <= 0) continue;
			ItemInstance rest(*in);
			if (!addToRange(rest, HOTBAR_FIRST, HOTBAR_FIRST + 8) && !addToRange(rest, STORAGE_FIRST, HOTBAR_FIRST - 1))
				minecraft->player->drop(new ItemInstance(rest), false);
			craftSlots->setItem(i, ItemInstance());
		}
		if (!carried.isNull()) {
			ItemInstance rest(carried);
			if (!addToRange(rest, HOTBAR_FIRST, HOTBAR_FIRST + 8) && !addToRange(rest, STORAGE_FIRST, HOTBAR_FIRST - 1))
				minecraft->player->drop(new ItemInstance(rest), false);
			carried = ItemInstance();
		}
	}
	minecraft->gui.inventoryUpdated();
}

void JavaInventoryScreen::serverSetSlot(int windowSlot, const ItemInstance& item)
{
	if (windowSlot == -1) { carried = item; return; }
	for (unsigned int i = 0; i < slots.size(); ++i) {
		const Slot& s = slots[i];
		if (s.windowSlot != windowSlot) continue;
		if (s.kind == GRID) craftSlots->setItem(s.index, item.isNull() ? ItemInstance() : item);
		else if (s.kind == RESULT) resultItem = item;
		// storage / armour slots are written by EaglerClient directly
		return;
	}
}

void JavaInventoryScreen::renderPlayer(float x, float y, float mx, float my)
{
	Player* player = minecraft->player;
	glPushMatrix();
	glTranslatef(x, y, -200);
	const float ss = 30 * ui;
	glScalef(-ss, ss, ss);
	glRotatef(180, 0, 0, 1);

	const float oybr = player->yBodyRot, oyr = player->yRot, oxr = player->xRot;
	const float xd = x - mx, yd = y - 50 * ui - my;
	glRotatef(135, 0, 1, 0);
	glRotatef(-135, 0, 1, 0);
	glRotatef(-Mth::atan(yd / 40.0f) * 20, 1, 0, 0);
	player->yBodyRot = Mth::atan(xd / 40.0f) * 20;
	player->yRot = Mth::atan(xd / 40.0f) * 40;
	player->xRot = -Mth::atan(yd / 40.0f) * 20;
	glTranslatef(0, player->heightOffset, 0);

	const float oWAP = player->walkAnimPos, oWAS = player->walkAnimSpeed, oWASO = player->walkAnimSpeedO;
	player->walkAnimPos = 0; player->walkAnimSpeed = player->walkAnimSpeedO = 0;
	EntityRenderDispatcher* rd = EntityRenderDispatcher::getInstance();
	rd->playerRotY = 180;
	rd->render(player, 0, 0, 0, 0, 1);
	player->walkAnimPos = oWAP; player->walkAnimSpeed = oWAS; player->walkAnimSpeedO = oWASO;

	player->yBodyRot = oybr; player->yRot = oyr; player->xRot = oxr;
	glPopMatrix();
}

void JavaInventoryScreen::drawCount(const ItemInstance& item, int x, int y)
{
	if (item.count <= 1) return;
	char buf[8];
	sprintf(buf, "%d", item.count);
	const std::string str(buf);
	minecraft->font->drawShadow(str, (float)(x + 17 - minecraft->font->width(str)), (float)(y + 9), 0xffffffff);
}

void JavaInventoryScreen::render(int xm, int ym, float a)
{
	lastMouseX = xm; lastMouseY = ym;
	hovered = slotAt(xm, ym);

	fillGradient(0, 0, width, height, 0xc0101010, 0xd0101010);   // Java's dark backdrop

	glPushMatrix();
	glTranslatef(left, top, 0);
	glScalef(ui, ui, 1);

	glColor4f2(1, 1, 1, 1);
	minecraft->textures->loadAndBindTexture(table ? "gui/java_crafting.png" : "gui/java_inventory.png");
	blit(0, 0, 0, 0, PANEL_W, PANEL_H);

	const int textCol = 0xff404040;
	if (table) {
		minecraft->font->draw("Crafting", 28.0f, 6.0f, textCol, false);
		minecraft->font->draw("Inventory", 8.0f, (float)(PANEL_H - 96 + 2), textCol, false);
	} else {
		minecraft->font->draw("Crafting", 97.0f, 8.0f, textCol, false);
	}
	glPopMatrix();

	if (!table) {
		// the model needs depth testing; the GUI below never wrote useful depth
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable2(GL_DEPTH_TEST);
		renderPlayer(left + 51 * ui, top + 75 * ui, (float)xm, (float)ym);
		glDisable2(GL_DEPTH_TEST);
	}

	glPushMatrix();
	glTranslatef(left, top, 0);
	glScalef(ui, ui, 1);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (unsigned int i = 0; i < slots.size(); ++i) {
		const Slot& s = slots[i];
		ItemInstance item = get(s);
		if (!item.isNull()) {
			ItemRenderer::renderGuiItem(minecraft->font, minecraft->textures, &item, (float)s.x, (float)s.y, true);
			ItemRenderer::renderGuiItemDecorations(&item, (float)s.x, (float)s.y);
			drawCount(item, s.x, s.y);
		}
		if ((int)i == hovered) fill(s.x, s.y, s.x + 16, s.y + 16, 0x80ffffff);
	}
	glPopMatrix();

	// carried item follows the cursor
	if (!carried.isNull()) {
		glPushMatrix();
		glTranslatef((float)xm - 8 * ui, (float)ym - 8 * ui, 0);
		glScalef(ui, ui, 1);
		ItemRenderer::renderGuiItem(minecraft->font, minecraft->textures, &carried, 0, 0, true);
		ItemRenderer::renderGuiItemDecorations(&carried, 0, 0);
		drawCount(carried, 0, 0);
		glPopMatrix();
	} else if (hovered >= 0) {
		// tooltip with the item name
		ItemInstance item = get(slots[hovered]);
		if (!item.isNull()) {
			std::string name = item.getName();
			const int w = minecraft->font->width(name);
			int tx = xm + 8, ty = ym - 12;
			if (tx + w + 6 > width) tx = width - w - 6;
			if (ty < 2) ty = 2;
			fill(tx - 3, ty - 3, tx + w + 3, ty + 11, 0xf0100010);
			fill(tx - 3, ty - 3, tx + w + 3, ty - 2, 0x505000ff);
			fill(tx - 3, ty + 10, tx + w + 3, ty + 11, 0x505000ff);
			minecraft->font->drawShadow(name, (float)tx, (float)ty, 0xffffffff);
		}
	}
	glDisable2(GL_BLEND);
}
