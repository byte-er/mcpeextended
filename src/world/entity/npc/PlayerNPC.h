#ifndef NET_MINECRAFT_WORLD_ENTITY_NPC__PlayerNPC_H__
#define NET_MINECRAFT_WORLD_ENTITY_NPC__PlayerNPC_H__

#include "../PathfinderMob.h"
#include "../../item/ItemInstance.h"
#include "../../level/pathfinder/Path.h"
#include <deque>
#include <vector>
#include <string>

/**
 * "PlayerN": a Steve that plays the game on its own (/summon player).
 *
 * It gathers wood, mines a staircase down for stone and ore, crafts tools at
 * a crafting table, builds a house of its own design, makes a wheat farm and
 * an animal pen, chats with the others, helps them build, and goes inside at
 * night. It never reacts to the real player, who can't hurt it either; it
 * only goes away by dying.
 *
 * The brain is a planner that picks the next job from what it has and still
 * needs, and turns the job into steps (walk, break, place, wait, craft, lead
 * an animal) for the executor. Everything is world-position based and saved
 * with the entity (positions relative to it, so infinite worlds can move).
 */
class PlayerNPC : public PathfinderMob
{
	typedef PathfinderMob super;
public:
	PlayerNPC(Level* level);
	~PlayerNPC();

	int getEntityTypeId() const;
	int getMaxHealth() { return 20; }
	int getCreatureBaseType() const { return 0; } // not counted as a monster or animal
	bool removeWhenFarAway() { return false; }
	float getHeadHeight() { return 0.12f; } // y is at eye level, like a player
	bool hurt(Entity* source, int dmg);
	void die(Entity* source);
	void aiStep();
	ItemInstance* getCarriedItem() { return held.isNull() ? NULL : &held; }
	void windowShifted(int dx, int dz);

	void addAdditonalSaveData(CompoundTag* tag);
	void readAdditionalSaveData(CompoundTag* tag);

	std::string getNpcName() const;
	int number;
	void setNumber(int n);
	static const int DATA_NPC_NUMBER = 16;

	/** /summon player: a new numbered NPC next to (x, y, z), announced like a player joining. */
	static PlayerNPC* spawn(Level* level, float x, float y, float z);

	// ---- for the others ----
	bool isBuilding() const { return job == JOB_BUILD || job == JOB_EXTRA; }
	bool claimBuildBlock(int& x, int& y, int& z, int& tile, int& data);
	int  homeX, homeY, homeZ;
	bool hasHome;

protected:
	void updateAi();

private:
	enum Job { JOB_NONE, JOB_WOOD, JOB_MINE, JOB_SEEDS, JOB_CRAFT, JOB_TABLE, JOB_BUILD, JOB_HELP,
	           JOB_FARM, JOB_HARVEST, JOB_PEN, JOB_EXTRA, JOB_NIGHT, JOB_WANDER, JOB_FIGHT, JOB_WATER };
	enum Inv { I_LOG, I_PLANK, I_STICK, I_COBBLE, I_DIRT, I_SEEDS, I_WHEAT, I_COAL, I_IRON, I_FENCE,
	           I_GATE, I_DOOR, I_TORCH, I_BREAD, I_SAND, I_TABLE, I_FURNACE, I_COUNT };
	struct Step {
		enum Kind { MOVE, BREAK, PLACE, TILL, WAIT, CRAFT, LEAD, SAY } kind;
		int x, y, z, tile, data, ticks;
		std::string text;
	};
	struct Planned { int x, y, z, tile, data; };

	// planner
	void think();
	void startJob(int j);
	bool planWood();
	bool planMine();
	bool planSeeds();
	bool planTable();
	bool planCraft();
	bool planBuild(bool extra);
	bool planHelp();
	bool planFarm();
	bool planHarvest();
	bool planPen();
	bool planNight();
	bool planExplore();
	bool planFinish(bool extra);
	int  fillBuildSteps(PlayerNPC* owner, int maxBlocks);
	void planWander();
	void chooseHome();
	void makeHouseDesign(int seed);
	void makeExtraDesign(int seed);
	bool reserved(int x, int z, int margin) const;

	// executor
	void runStep();
	void popStep();
	int  approach(int x, int y, int z);
	bool walkTo(float x, float y, float z, float reach);
	bool standSpotFor(int x, int y, int z, int& sx, int& sy, int& sz);
	bool standable(int x, int y, int z);
	void followRoute();
	void lookAtBlock(int x, int y, int z);
	bool inReach(int x, int y, int z) const;
	float reach() const;
	int  digTicks(int tile);
	void collect(int tile, int data);
	int  itemFor(int tile) const;
	void setHeld(int tile, bool block);
	void craftAll(bool atTable);
	bool wantTable() const;
	bool afford(int planks, int cobble, int logs);
	void push(Step::Kind k, int x = 0, int y = 0, int z = 0, int tile = 0, int data = 0, int ticks = 0);

	// chat
	void say(const std::string& text, int topic = -1);
	void chatTick();
	void reply(int topic, PlayerNPC* to);
	std::vector<PlayerNPC*> others();

	int job;
	int jobTicks;
	std::deque<Step> steps;
	int inv[I_COUNT];
	int axe, pick, hoe, sword; // 0 none, 1 wood, 2 stone
	ItemInstance held;

	Path route;
	int routeTx, routeTy, routeTz, routeAge, routeFails, stuckTicks;
	float lastX, lastZ;
	int digProgress, digNeeded, swingTimer;

	// places
	int tableX, tableY, tableZ; bool hasTable;
	bool houseDone, farmDone, penDone, waterPlaced;
	int extrasBuilt;
	std::vector<Planned> plan; unsigned int planPos; int planSeed;
	int planX, planY, planZ, planW, planD;     // footprint of the current build (for keeping others out)
	int farmX, farmY, farmZ, farmW, farmD;
	int penX, penY, penZ, penW, penD;
	int mineX, mineY, mineZ, mineDir, mineDepth;
	int fightTarget;
	int helpingId;
	bool hasWater;

	// chat
	int chatCooldown;
	std::string pendingLine; int pendingTopic; int pendingDelay;
	int lastTopicSaid;
	int keepLogs;
	int seedTries;
	bool mineUnsafe(int x, int y, int z);
	int directTicks;
	bool standValid; int standX, standY, standZ, standForX, standForY, standForZ;
	std::vector<int> standExcluded;
	bool hasMine; int mineFails;
	int buildPasses; bool announcedBuild;
	struct Bad { int x, y, z, time; };
	std::vector<Bad> badSpots; // places it couldn't reach lately
	void markBad(int x, int y, int z);
	bool isBad(int x, int y, int z) const;
	int exploreCount; float exploreAngle;
	int lastLoggedJob, lastLogTime;
};

#endif /*NET_MINECRAFT_WORLD_ENTITY_NPC__PlayerNPC_H__*/
