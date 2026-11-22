#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchCreativeScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchCreativeScreen_H__

#include "../../Screen.h"
#include "../../../../world/item/ItemInstance.h"
#include <vector>

namespace Touch {

/**
 * Creative inventory as in Pocket Edition 0.9.0: a close button and four
 * category tabs down the left (items, tools, decoration, blocks), and an
 * 8 column grid of items that scrolls with a finger drag. Tapping an item
 * puts it in the hotbar; the screen stays open until X is tapped.
 */
class CreativeScreen : public Screen
{
	typedef Screen super;
public:
	CreativeScreen();
	~CreativeScreen();

	void init();
	void setupPositions();
	void tick();
	void render(int xm, int ym, float a);
	void removed();
	bool handleBackEvent(bool isDown);
	bool renderGameBehind() { return true; }

	enum { TAB_ITEMS, TAB_TOOLS, TAB_DECORATION, TAB_BLOCKS, NUM_TABS };
	/** Category tab of a creative item and its position in 0.9.0's order. */
	static int tabFor(const ItemInstance* item, int* order);

protected:
	void mouseClicked(int x, int y, int buttonNum);
	void mouseReleased(int x, int y, int buttonNum);
	void keyPressed(int eventKey);

private:
	struct Entry { int slot; };              // creative inventory slot

	void buildTab();
	int cellAt(int x, int y);                // entry index or -1
	float maxScroll() const;
	void pick(int entry);
	void renderPanel();
	void renderTab(int tab, bool selected);
	void renderCell(float x, float y, float s);
	void renderItem(const ItemInstance* item, float cx, float cy, float size);
	bool inside(float px, float py, float x, float y, float w, float h) const {
		return px >= x && py >= y && px < x + w && py < y + h;
	}

	static int lastTab;
	int tab;
	std::vector<Entry> entries;
	ItemInstance tabIcons[NUM_TABS];

	// layout (GUI units)
	float panelX, panelY, panelW, panelH;
	float unit;                               // tab column scale
	float closeX, closeY, closeS;
	float tabX, tabY0, tabW, tabH, tabPitch;
	float contentX, contentY, contentW, contentH;
	float gridX, gridY, pitch;
	int columns;

	// scrolling
	float scroll, scrollVel;
	bool dragging, dragMoved, closePressed;
	int downX, downY;
	float downScroll;
	int lastDragY;
	int lastMouseY;
};

}

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchCreativeScreen_H__*/
