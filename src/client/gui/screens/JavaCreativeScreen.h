#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__JavaCreativeScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__JavaCreativeScreen_H__

#include "../Screen.h"
#include "../../../world/item/ItemInstance.h"
#include <vector>
#include <string>

/**
 * Java Edition creative inventory (PC, mouse and keyboard): category tabs
 * above the panel, a scrolling 9x5 item grid, the hotbar underneath, a
 * stack on the cursor, and a "Saved Hotbars" tab below (Java 1.12's saved
 * toolbars: hover the tab's rows and click to load; C + 1-9 saves the
 * current hotbar, X + 1-9 loads one).
 *
 * Items come from the creative inventory list (slots >= 9); hotbar slots
 * link to them like everywhere else in this engine.
 */
class JavaCreativeScreen : public Screen
{
	typedef Screen super;
public:
	JavaCreativeScreen();
	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	void mouseClicked(int x, int y, int buttonNum);
	void mouseReleased(int x, int y, int buttonNum);
	void mouseEvent();
	void keyPressed(int eventKey);
	bool handleBackEvent(bool isDown);
	bool renderGameBehind() { return true; }
	void removed();

private:
	enum { TAB_BLOCKS, TAB_DECORATION, TAB_TOOLS, TAB_MISC, TAB_SAVED, NUM_TABS };
	struct Hotbar { int id[9]; int aux[9]; };

	void buildList();
	int  rows() const;
	int  maxScroll() const { int m = rows() - 5; return m > 0 ? m : 0; }
	void toGui(int sx, int sy, float& gx, float& gy) const;
	int  gridAt(float gx, float gy) const;     // list index or -1
	int  hotbarAt(float gx, float gy) const;   // 0..8 or -1
	int  tabAt(float gx, float gy) const;
	bool tabRect(int tab, float& x, float& y) const;
	const ItemInstance* listItem(int i);
	int  creativeSlotFor(int id, int aux);
	void linkHotbar(int h, int creativeSlot);
	void loadHotbars();
	void storeHotbars();
	void saveHotbar(int row);
	void restoreHotbar(int row);
	void drawItem(const ItemInstance* item, float x, float y);
	void drawTooltip(const std::string& text, int xm, int ym);

	static int lastTab;
	int tab;
	std::vector<int> list;       // creative inventory slots in the current tab
	int scroll;                  // first visible row
	bool draggingScroller;
	int heldSlot;                // creative slot on the cursor, -1 = none
	std::vector<Hotbar> saved;   // 9 saved hotbars
	ItemInstance tabIcons[NUM_TABS];
	float ui, left, top;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__JavaCreativeScreen_H__*/
