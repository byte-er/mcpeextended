#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__JavaInventoryScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__JavaInventoryScreen_H__

#include "../Screen.h"
#include "../../../world/item/ItemInstance.h"
#include <vector>

class CraftingContainer;
class FurnaceTileEntity;

/**
 * Java Edition style inventory ("Enable PC UI"): armour slots, player
 * preview, a 2x2 crafting grid titled "Crafting", 27 storage slots and the
 * hotbar, with a carried item on the cursor. With table = true it is the
 * 3x3 crafting table screen instead.
 *
 * Slots use Java's window numbering (inventory: result 0, grid 1-4,
 * armour 5-8, storage 9-35, hotbar 36-44; table: result 0, grid 1-9,
 * storage 10-36, hotbar 37-45; furnace: input 0, fuel 1, result 2,
 * storage 3-29, hotbar 30-38), so on Eaglercraft servers clicks are sent
 * as standard window clicks and the server does the crafting.
 */
class JavaInventoryScreen : public Screen
{
	typedef Screen super;
public:
	JavaInventoryScreen(bool table, int windowId = 0);
	/** Furnace screen: furnace = the block's tile entity (single player), or NULL on servers. */
	JavaInventoryScreen(FurnaceTileEntity* furnace, int windowId);
	~JavaInventoryScreen();

	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	void tick();
	void removed();
	void mouseClicked(int x, int y, int buttonNum);
	void mouseReleased(int x, int y, int buttonNum) {}
	void keyPressed(int eventKey);
	bool handleBackEvent(bool isDown);
	bool renderGameBehind() { return true; }

	/** Eaglercraft: the server set a slot of this window (-1 = cursor). */
	void serverSetSlot(int windowSlot, const ItemInstance& item);
	int getWindowId() const { return windowId; }
	bool isTable() const { return table; }
	bool isFurnace() const { return furnaceMode; }
	/** Eaglercraft: window property (furnace burn / cook progress). */
	void serverSetProperty(int prop, int value);

	/** Keeps the hotbar on inventory slots 36-44 (Java layout). */
	static void fixHotbarLinks(class Inventory* inv);
	/** The Java-style screen currently shown, if any. */
	static JavaInventoryScreen* active;

private:
	enum Kind { STORAGE, GRID, RESULT, ARMOR, FURNACE };
	struct Slot { int x, y; Kind kind; int index; int windowSlot; };

	ItemInstance get(const Slot& s);
	void set(const Slot& s, const ItemInstance& item);
	int slotAt(int x, int y);           // screen coords -> slot index or -1
	void toGui(int sx, int sy, float& gx, float& gy);
	void click(int slot, int button, bool shift);
	void craft(bool toInventory);
	void updateResult();
	bool addToRange(ItemInstance& item, int from, int to); // merge then fill, storage slot indices
	void quickMove(int slot);
	void sendClick(int windowSlot, int button, int mode, const ItemInstance& clicked);
	void renderPlayer(float x, float y, float mx, float my);
	void drawCount(const ItemInstance& item, int x, int y);
	static int maxStack(const ItemInstance& i);
	static bool same(const ItemInstance& a, const ItemInstance& b);
	FurnaceTileEntity* liveFurnace();
	bool moveIntoFurnace(ItemInstance& item, int furnaceSlot);
	void renderFurnaceProgress();

	bool table;
	bool furnaceMode;
	FurnaceTileEntity* furnace;      // single player; checked against the level before use
	int furnaceX, furnaceY, furnaceZ;
	ItemInstance furnaceItems[3];    // servers: what the server says is in the furnace
	int props[4];                    // servers: burn time, fuel burn time, cook time, total cook time
	int windowId;
	int gridW;
	CraftingContainer* craftSlots;
	ItemInstance resultItem;
	ItemInstance carried;
	std::vector<Slot> slots;
	float ui;              // GUI scale factor applied to the 176x166 panel
	float left, top;       // panel origin, in screen GUI pixels
	int hovered;
	int lastMouseX, lastMouseY;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__JavaInventoryScreen_H__*/
