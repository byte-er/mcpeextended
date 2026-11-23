#ifndef NET_MINECRAFT_WORLD_ENTITY_NPC__PlayerNPC_H__
#define NET_MINECRAFT_WORLD_ENTITY_NPC__PlayerNPC_H__

#include "../PathfinderMob.h"
#include "../../item/ItemInstance.h"
#include "../../level/pathfinder/Path.h"
#include "NpcTown.h"
#include <deque>
#include <vector>
#include <string>

/**
 * "PlayerN": a Steve that plays the game on its own (/summon player).
 *
 * It gathers wood, mines and goes caving, crafts tools at a crafting table it
 * carries around, smelts iron, builds a house of its own design, makes a farm
 * and an animal pen, chats with the others, helps them build, and goes inside
 * at night. Every one has a play style (builder, miner, lumberjack, farmer,
 * explorer, and now and then a griefer) and they react to each other. With
 * enough of them they found a town (see NpcTown) and work on it together.
 * It never reacts to the real player, who can't hurt or push it either; it
 * only goes away by dying.
 *
 * The brain is a planner that picks the next job from what it has and still
 * needs, and turns the job into steps (walk, break, place, wait, craft, lead
 * an animal, use a chest) for the executor. Everything is world-position based
 * and saved with the entity (positions relative to it, so infinite worlds can
 * move).
 */
class PlayerNPC : public PathfinderMob
{
	typedef PathfinderMob super;
	friend class NpcTown;
public:
	PlayerNPC(Level* level);
	~PlayerNPC();

	int getEntityTypeId() const;
	int getMaxHealth() { return 20; }
	int getCreatureBaseType() const { return 0; } // not counted as a monster or animal
	bool removeWhenFarAway() { return false; }
	float getHeadHeight() { return 0.12f; } // y is at eye level, like a player
	bool isPushable() { return false; }     // everyone walks right through them
	void push(Entity* e) {}
	void push(float xa, float ya, float za) {}
	bool hurt(Entity* source, int dmg);
	void die(Entity* source);
	void aiStep();
	ItemInstance* getCarriedItem() { return held.isNull() ? NULL : &held; }
	void windowShifted(int dx, int dz);
	float getWalkingSpeedModifier();

	void addAdditonalSaveData(CompoundTag* tag);
	void readAdditionalSaveData(CompoundTag* tag);

	std::string getNpcName() const;
	int number;
	void setNumber(int n);
	static const int DATA_NPC_NUMBER = 16;

	/** /summon player: a new numbered NPC next to (x, y, z), announced like a player joining. */
	static PlayerNPC* spawn(Level* level, float x, float y, float z);

	enum Role { ROLE_BUILDER, ROLE_MINER, ROLE_LUMBERJACK, ROLE_FARMER, ROLE_EXPLORER, ROLE_GRIEFER, ROLE_COUNT };
	int role;

	// ---- for the others ----
	bool isBuilding() const { return job == JOB_BUILD || job == JOB_EXTRA; }
	bool claimBuildBlock(int& x, int& y, int& z, int& tile, int& data);
	void heardGrief(PlayerNPC* griefer, int victim, int x, int y, int z, bool saw);
	int  grudgeAgainst(int number) const;
	bool goodTools() const { return pick >= 2; }
	int  homeX, homeY, homeZ;
	bool hasHome;
	bool houseDone;
	int  planX, planY, planZ, planW, planD;     // footprint of the current build (for keeping others out)
	int  farmX, farmY, farmZ, farmW, farmD;
	int  penX, penY, penZ, penW, penD;
	int  plotI, plotJ; bool hasPlot;

protected:
	void updateAi();

private:
	enum Job { JOB_NONE, JOB_WOOD, JOB_MINE, JOB_SEEDS, JOB_CRAFT, JOB_TABLE, JOB_BUILD, JOB_HELP,
	           JOB_FARM, JOB_HARVEST, JOB_PEN, JOB_EXTRA, JOB_NIGHT, JOB_WANDER, JOB_FIGHT, JOB_WATER,
	           JOB_CAVE, JOB_GRIEF, JOB_STORE, JOB_PROJECT, JOB_SMELT, JOB_FLEE, JOB_COUNT };
	// (new kinds go at the end: the counts are saved by index)
	enum Inv { I_LOG, I_PLANK, I_STICK, I_COBBLE, I_DIRT, I_SEEDS, I_WHEAT, I_COAL, I_IRON, I_FENCE,
	           I_GATE, I_DOOR, I_TORCH, I_BREAD, I_SAND, I_TABLE, I_FURNACE,
	           I_GOLD, I_DIAMOND, I_REDSTONE, I_LAPIS, I_INGOT, I_GRAVEL, I_STONEBRICK, I_GLASS, I_SAPLING, I_COUNT };
	struct Step {
		enum Kind { MOVE, BREAK, PLACE, TILL, WAIT, CRAFT, LEAD, SAY, PILLAR, CHEST, SMELT, PICKUP } kind;
		int x, y, z, tile, data, ticks;
		int proj;            // town project the block belongs to (-1: none)
		std::string text;
	};
	typedef NpcPlanned Planned;

	// planner
	void think();
	void startJob(int j);
	bool planWood();
	bool planMine();
	bool planSeeds();
	bool planCraft();
	bool planBuild(bool extra);
	bool planHelp();
	bool planFarm();
	bool planHarvest();
	bool planPen();
	bool planNight();
	bool planExplore();
	bool planFinish(bool extra);
	bool planCave();
	bool planCaveStep();
	bool planGrief();
	bool planStore(bool deposit);
	bool planProject();
	bool planSmelt();
	bool planFlee(Entity* from);
	bool planBasics();
	bool planTownWork();
	bool needsTableFor() const;
	bool tableNearby(int& tx, int& ty, int& tz);
	int  fillBuildSteps(PlayerNPC* owner, int maxBlocks);
	void planWander();
	void chooseHome();
	void makeHouseDesign(int seed);
	void makeExtraDesign(int seed);
	bool reserved(int x, int z, int margin) const;
	bool wantsDeposit() const;
	bool missingFor(const std::vector<Planned>& plan, unsigned int from, int& needP, int& needC, int& needL, int& needO, int maxScan);

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
	void smeltAll();
	bool afford(int planks, int cobble, int logs);
	void push(Step::Kind k, int x = 0, int y = 0, int z = 0, int tile = 0, int data = 0, int ticks = 0);
	void pushFront(Step::Kind k, int x = 0, int y = 0, int z = 0, int tile = 0, int data = 0, int ticks = 0);
	bool canHarvest(int tile) const;
	int  invTile(int inv, int& aux) const; // what goes into a chest for an inventory kind
	void useChest(const Step& s);

	// how it moves and looks (the small things players do)
	void microTick();
	void applyGlance();
	void setSneak(bool on);

	// chat
	void say(const std::string& text, int topic = -1);
	void chatTick();
	void reply(int topic, PlayerNPC* to);
	std::vector<PlayerNPC*> others();
	PlayerNPC* nearestOther(float maxDist);

	int job;
	int jobTicks;
	std::deque<Step> steps;
	int inv[I_COUNT];
	int axe, pick, hoe, sword; // 0 none, 1 wood, 2 stone, 3 iron, 4 diamond
	ItemInstance held;

	Path route;
	int routeTx, routeTy, routeTz, routeAge, routeFails, stuckTicks;
	float lastX, lastZ;
	int digProgress, digNeeded, swingTimer;

	// places
	int tableX, tableY, tableZ; bool hasTable;     // a table standing somewhere (the house's, usually)
	bool farmDone, penDone, waterPlaced;
	int extrasBuilt;
	std::vector<Planned> plan; unsigned int planPos; int planSeed;
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
	int progTimer; float progDist;
	std::vector<Step> digQueue;
	bool mineUnsafe(int x, int y, int z, bool& caveDrop);
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

	// caving
	struct Waypoint { int x, y, z; };
	std::vector<Waypoint> caveTrail;
	int caveMoves, caveTorchX, caveTorchY, caveTorchZ, caveCooldown;
	bool returning;

	// play style and the others
	struct Grudge { int number, amount; };
	std::vector<Grudge> grudges;
	int griefCooldown;
	int lastGriefVictim;
	int projectId;
	int storeCooldown;
	int timeAlive;

	// micro movements
	float moveYaw, glance, glanceTarget; int glanceTicks; bool glancing;
	int hopHabit, hopTimer;
	int idleFidget;
	int swapTicks; ItemInstance swapBack;
	bool sneakNow, sneakWas;
	int greetCooldown;
	bool sprinting;
};

#endif /*NET_MINECRAFT_WORLD_ENTITY_NPC__PlayerNPC_H__*/
