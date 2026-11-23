#ifndef NET_MINECRAFT_WORLD_ENTITY_NPC__NpcTown_H__
#define NET_MINECRAFT_WORLD_ENTITY_NPC__NpcTown_H__

#include <vector>
#include <map>
#include <string>

class Level;
class PlayerNPC;
class CompoundTag;
class ChestTileEntity;

/** One block of a build plan. data -1: only fill where there's nothing, -2: optional. */
struct NpcPlanned { int x, y, z, tile, data; };

/**
 * A town project everyone can work on: the plaza, a road segment, the town hall,
 * the community farm, watch towers, the town wall, levelling an empty plot.
 * The block plan is made from the project's numbers (never from the terrain as
 * it is now), so a half finished project comes out the same after a reload.
 */
struct NpcProject {
	enum Type { PLAZA, ROAD, HALL, FARM, TOWER, WALL, LEVEL, STORAGE };
	int type;
	int i, j, dir;           // plot cell / road segment start and direction
	int seed;
	unsigned int pos;        // next block to hand out
	int passes;              // second pass picks up what was skipped
	bool done;
	int workers;             // NPCs on it right now (counted each tick)
	std::vector<NpcPlanned> plan;
	bool built;              // plan made
};

/**
 * The shared side of the Player NPCs.
 *
 * Always: a per-tick list of the NPCs (so nobody scans the entity list over
 * and over), a budget for path finding, chat pacing, and the grief log.
 *
 * With enough NPCs (TOWN_MIN) they found a town: a grid of 16 block cells
 * with 3 wide roads between them, a plot for everybody's house, a plaza with
 * the storage chests, and a list of big projects (roads, town hall, farm,
 * towers, a wall) that start once the town has ground a little (tools and
 * stock), like an SMP. Positions are kept in world coordinates (infinite
 * worlds slide their window) and saved with every NPC.
 */
class NpcTown
{
public:
	static const int TOWN_MIN = 8;   // NPCs it takes to found a town
	static const int CELL = 16;      // road grid spacing

	static NpcTown* get(Level* level);
	/** Blocks somebody put there (never knocked down by projects or clearing). */
	static bool artificial(int tile);
	static void levelGone(Level* level);

	explicit NpcTown(Level* level);

	// ---- every tick (called by the first NPC that ticks) ----
	void tick();
	const std::vector<PlayerNPC*>& npcs();
	int population();
	PlayerNPC* byNumber(int number);
	bool takePathToken();
	bool mayChat(int importance); // 0 small talk, 1 normal, 2 always

	// ---- town ----
	bool active;
	int phase;                 // 0 grinding, 1 big projects
	int cells;                 // grid half size in cells (plots -cells..cells-1)
	long founded;
	int wx, wz;                // world coords of the centre road crossing
	std::map<long, int> heights;   // road crossing heights (by cell corner)
	std::map<long, int> plotOwner; // NPC number per plot
	std::vector<NpcProject> projects;
	int storageW, storageY;    // storage chests: along the plaza's north edge

	int cx() const;            // centre crossing in level coords
	int cz() const;
	int crossingHeight(int k, int l);
	int plotLevel(int i, int j);
	void plotRect(int i, int j, int& x0, int& z0, int& w, int& d) const; // level coords, inside the roads
	bool plotFor(PlayerNPC* npc, int& i, int& j);
	bool onRoad(int x, int z, int margin) const;
	bool inTown(int x, int z, int margin) const;
	bool inProjectArea(int x, int z, int margin);

	NpcProject* pickProject(PlayerNPC* npc);
	bool claimBlock(NpcProject* p, NpcPlanned& out);
	void buildPlan(NpcProject& p);
	int projectIndex(NpcProject* p) const { return (int)(p - &projects[0]); }

	// ---- storage chests ----
	int storageCount() const { return 4; }
	void storagePos(int k, int& x, int& y, int& z);
	ChestTileEntity* chest(int k);
	int stock(int id, int aux = -1);
	int deposit(int id, int aux, int count);
	int withdraw(int id, int aux, int count);
	bool storageReady();

	// ---- griefing ----
	struct Grief { int griefer, victim, x, y, z; long time; };
	std::vector<Grief> griefs;
	void reportGrief(PlayerNPC* griefer, int victimNumber, int x, int y, int z);

	void save(CompoundTag* tag);
	void load(CompoundTag* tag);
	bool loaded;

private:
	void found();
	void grow();
	void planProjects();
	void addProject(int type, int i, int j, int dir);
	long cornerKey(int k, int l) const { return ((long)(k + 4096) << 16) | (long)(l + 4096); }
	void ensureCells(int want);

	Level* level;
	std::vector<PlayerNPC*> list;
	long listTime;
	int listCount;
	long pathTime;
	int pathTokens;
	long lastChat;
	int chatLines;          // lines in the last little while
	long lastProjectCheck;
	int plannedFor;         // population the project list was made for
};

#endif /*NET_MINECRAFT_WORLD_ENTITY_NPC__NpcTown_H__*/
