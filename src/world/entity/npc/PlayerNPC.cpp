#include "PlayerNPC.h"
#include "../EntityTypes.h"
#include "../player/Player.h"
#include "../projectile/Arrow.h"
#include "../../level/Level.h"
#include "../../level/LevelSettings.h"
#include "../../level/storage/LevelData.h"
#include "../../level/tile/Tile.h"
#include "../../level/tile/LevelEvent.h"
#include "../../level/material/Material.h"
#include "../../item/Item.h"
#include "../../phys/Vec3.h"
#include "../../../nbt/CompoundTag.h"
#include "../../../util/Mth.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

// ---------------------------------------------------------------------------
// chat lines
// ---------------------------------------------------------------------------
namespace {
enum Topic { T_WOOD, T_STONE, T_COAL, T_IRON, T_BUILD, T_BUILT, T_FARM, T_PEN, T_NIGHT, T_MORNING,
             T_HELLO, T_HELP, T_BANTER, T_HURT, T_DEATH, T_CRAFT, T_TOOLS, T_HARVEST, T_COUNT };

const char* const SAY_LINES[T_COUNT][6] = {
	{ "getting some wood", "need more logs", "chopping trees brb", "anyone seen birch around here", "wood run", "gonna grab some logs" },
	{ "going mining", "digging down for stone", "need cobble", "strip mining time", "gonna make a little mine", "need stone for my house" },
	{ "found coal", "coal!", "got some coal", "nice coal vein", "torches incoming", "ooh coal" },
	{ "IRON", "found iron!!", "got iron ore", "iron down here", "finally some iron", "iron :D" },
	{ "starting my house", "building a base", "gonna build here", "house time", "this spot looks good for a house", "building lol" },
	{ "house done", "finished my house", "my house is done", "base is done :)", "ok house finished", "done building" },
	{ "making a wheat farm", "starting a farm", "farm time", "planting wheat", "gonna farm a bit", "need food so farm" },
	{ "making an animal pen", "building a pen", "gonna get some cows in a pen", "animal farm time", "fencing in some animals", "pen time" },
	{ "its getting dark", "night already?", "going inside", "mobs are out", "gonna stay in for the night", "night lol" },
	{ "morning", "its day again", "ok back to work", "sun is up", "good morning", "day time" },
	{ "hi", "hey", "hello", "yo", "hii", "sup" },
	{ "anyone need help", "ill help build", "need a hand?", "helping out", "lemme help", "want help with that" },
	{ "this world is nice", "lol", "what are you guys doing", "brb", "anyone got food", "wish we had diamonds" },
	{ "ouch", "ow", "almost died lol", "help", "mobs everywhere", "that hurt" },
	{ "rip", "noo", "f", "rip lol", "oof", "rip bro" },
	{ "crafting", "making tools", "need a crafting table", "crafting stuff", "making sticks", "one sec crafting" },
	{ "got a stone pickaxe", "stone tools now", "upgraded my tools", "new pickaxe", "finally stone tools", "got my tools" },
	{ "harvesting wheat", "wheat is ready", "making bread", "farm is growing nicely", "got wheat", "replanting" },
};

const char* const REPLY_LINES[T_COUNT][4] = {
	{ "ok", "can you get me some too", "theres trees over by my house", "nice" },
	{ "careful down there", "same", "get me some cobble", "ok" },
	{ "nice", "share some?", "lucky", "i need coal" },
	{ "no way", "nice!!", "where", "lucky" },
	{ "nice spot", "cool", "ill build next to you", "ok" },
	{ "looks good", "nice house", "cool", "nice" },
	{ "nice", "ill help with the farm", "we need food", "good idea" },
	{ "lol", "nice", "get some sheep too", "cool" },
	{ "same", "yeah", "zombies lol", "ok gn" },
	{ "morning", "gm", "finally", "yay" },
	{ "hi", "hey", "hello", "welcome" },
	{ "sure", "thanks", "ok", "yes please" },
	{ "lol", "idk", "same", "fr" },
	{ "you ok?", "lol", "run", "careful" },
	{ "rip", "f", "noo", "rip" },
	{ "ok", "", "nice", "" },
	{ "nice", "noice", "", "cool" },
	{ "nice", "can i have bread", "", "yum" },
};

int s_lastChatTime = -100000;   // level time of the last line from any NPC
}

// ---------------------------------------------------------------------------

static int groundY(Level* level, int x, int z)
{
	int y = level->getHeightmap(x, z);
	if (y >= Level::DEPTH) y = Level::DEPTH - 1;
	while (y > 1 && !level->getMaterial(x, y - 1, z)->blocksMotion()) y--;
	return y; // first non-solid block above the ground
}

static bool soft(Level* level, int x, int y, int z)
{
	// things a player walks through or breaks without thinking: air, plants, snow
	const int t = level->getTile(x, y, z);
	if (t == 0) return true;
	const Material* m = level->getMaterial(x, y, z);
	return !m->blocksMotion() && !m->isLiquid();
}

PlayerNPC::PlayerNPC(Level* level)
:	super(level),
	number(0),
	homeX(0), homeY(0), homeZ(0), hasHome(false),
	job(JOB_NONE), jobTicks(0),
	axe(0), pick(0), hoe(0), sword(0),
	routeTx(0), routeTy(0), routeTz(0), routeAge(0), routeFails(0), stuckTicks(0),
	lastX(0), lastZ(0), digProgress(0), digNeeded(0), swingTimer(0),
	tableX(0), tableY(0), tableZ(0), hasTable(false),
	houseDone(false), farmDone(false), penDone(false), waterPlaced(false), extrasBuilt(0),
	planPos(0), planSeed(0), planX(0), planY(0), planZ(0), planW(0), planD(0),
	farmX(0), farmY(0), farmZ(0), farmW(0), farmD(0),
	penX(0), penY(0), penZ(0), penW(0), penD(0),
	mineX(0), mineY(0), mineZ(0), mineDir(0), mineDepth(0),
	fightTarget(0), helpingId(0), hasWater(false),
	chatCooldown(200), pendingTopic(-1), pendingDelay(0), lastTopicSaid(-1),
	keepLogs(0), seedTries(0), progTimer(0), progDist(1e9f), directTicks(0), standValid(false), standX(0), standY(0), standZ(0), standForX(0), standForY(0), standForZ(0), hasMine(false), mineFails(0), buildPasses(0), announcedBuild(false), exploreCount(0), exploreAngle(0), lastLoggedJob(-1), lastLogTime(0)
{
	entityRendererId = ER_NPC_RENDERER;
	textureName = "mob/char.png";
	setSize(0.6f, 1.8f);
	heightOffset = 1.62f;
	runSpeed = 0.75f;
	for (int i = 0; i < I_COUNT; ++i) inv[i] = 0;
	entityData.define(DATA_NPC_NUMBER, (SynchedEntityData::TypeInt) 0); // LAN clients get the name from this
}

void PlayerNPC::setNumber(int n)
{
	number = n;
	entityData.set(DATA_NPC_NUMBER, (SynchedEntityData::TypeInt) n);
}

PlayerNPC::~PlayerNPC() {}

int PlayerNPC::getEntityTypeId() const { return MobTypes::PlayerNPC; }

std::string PlayerNPC::getNpcName() const
{
	char buf[32];
	sprintf(buf, "Player%d", number ? number : entityData.getInt(DATA_NPC_NUMBER));
	return buf;
}

PlayerNPC* PlayerNPC::spawn(Level* level, float x, float y, float z)
{
	PlayerNPC* npc = new PlayerNPC(level);
	npc->setNumber(++level->getLevelData()->npcCounter);
	npc->moveTo(x, y + npc->heightOffset, z, level->random.nextFloat() * 360, 0);
	level->addEntity(npc);
	level->chatMessage("[yellow]" + npc->getNpcName() + " joined the game[/yellow]");
	// the others notice, the new one says hi a bit later
	std::vector<PlayerNPC*> o = npc->others();
	if (!o.empty() && level->random.nextInt(2) == 0) o[level->random.nextInt((int)o.size())]->reply(T_HELLO, npc);
	if (level->random.nextInt(3) != 0) { npc->pendingLine = SAY_LINES[T_HELLO][level->random.nextInt(6)]; npc->pendingTopic = -1; npc->pendingDelay = 60 + level->random.nextInt(140); }
	return npc;
}

std::vector<PlayerNPC*> PlayerNPC::others()
{
	std::vector<PlayerNPC*> out;
	for (unsigned int i = 0; i < level->entities.size(); ++i) {
		Entity* e = level->entities[i];
		if (e != this && !e->removed && e->getEntityTypeId() == MobTypes::PlayerNPC && e->isAlive()) out.push_back((PlayerNPC*)e);
	}
	return out;
}

// ---------------------------------------------------------------------------
// damage: the real player can't hurt them; everything else can
// ---------------------------------------------------------------------------
bool PlayerNPC::hurt(Entity* source, int dmg)
{
	// the real player can't hurt them (hands, weapons or arrows)
	if (source && source->isPlayer()) return false;
	if (source && source->getEntityTypeId() == EntityTypes::IdArrow) {
		Entity* owner = level->getEntity(((Arrow*)source)->ownerId);
		if (!owner || owner->isPlayer()) return false;
	}
	const bool hit = super::hurt(source, dmg);
	if (hit && source && source->isMob() && source->isAlive()) {
		fightTarget = source->entityId;
		if (job != JOB_FIGHT) { steps.clear(); job = JOB_FIGHT; jobTicks = 0; }
		if (random.nextInt(3) == 0) say(SAY_LINES[T_HURT][random.nextInt(6)], T_HURT);
	}
	return hit;
}

void PlayerNPC::die(Entity* source)
{
	if (!level->isClientSide && health <= 0) {
		std::string cause = " died";
		if (source && source->isMob()) {
			const int t = source->getEntityTypeId();
			const char* n = t == MobTypes::Zombie ? "Zombie" : t == MobTypes::Skeleton ? "Skeleton" : t == MobTypes::Spider ? "Spider"
				: t == MobTypes::Creeper ? "Creeper" : t == MobTypes::Enderman ? "Enderman" : t == MobTypes::PigZombie ? "Zombie Pigman" : "a mob";
			cause = std::string(" was slain by ") + n;
		} else if (isInLava()) cause = " tried to swim in lava";
		else if (isInWater() && airSupply <= 0) cause = " drowned";
		else if (fallDistance > 3) cause = " fell from a high place";
		level->chatMessage(getNpcName() + cause);
		if (getenv("MCPE_NPCLOG")) { printf("[npc] %s%s at %.1f,%.1f,%.1f job %d\n", getNpcName().c_str(), cause.c_str(), x, y - heightOffset, z, job); fflush(stdout); }
		std::vector<PlayerNPC*> o = others();
		if (!o.empty()) o[random.nextInt((int)o.size())]->reply(T_DEATH, this);
		// drops what it carried, like a player
		if (inv[I_LOG] > 0) spawnAtLocation(new ItemInstance(Tile::treeTrunk, Mth::Min(64, inv[I_LOG])), 0.5f);
		if (inv[I_PLANK] > 0) spawnAtLocation(new ItemInstance(Tile::wood, Mth::Min(64, inv[I_PLANK])), 0.5f);
		if (inv[I_COBBLE] > 0) spawnAtLocation(new ItemInstance(Tile::stoneBrick, Mth::Min(64, inv[I_COBBLE])), 0.5f);
		if (inv[I_WHEAT] > 0) spawnAtLocation(new ItemInstance(Item::wheat, inv[I_WHEAT]), 0.5f);
		if (inv[I_COAL] > 0) spawnAtLocation(new ItemInstance(Item::coal, inv[I_COAL]), 0.5f);
		if (pick > 0) spawnAtLocation(new ItemInstance(pick == 2 ? Item::pickAxe_stone : Item::pickAxe_wood), 0.5f);
	}
	super::die(source);
}

// ---------------------------------------------------------------------------
// main loop
// ---------------------------------------------------------------------------
void PlayerNPC::aiStep()
{
	super::aiStep();
	if (swingTimer > 0) swingTimer--;
}

void PlayerNPC::updateAi()
{
	noActionTime = 0;
	xxa = 0;
	yya = 0;
	jumping = false;
	if (level->isClientSide) return;

	chatTick();
	++jobTicks;

	// players heal while fed; eats some bread when hurt
	if (health < getMaxHealth() && health > 0 && (tickCount % 80) == 0) {
		heal(1);
		if (health < 12 && inv[I_BREAD] > 0 && random.nextInt(4) == 0) { inv[I_BREAD]--; heal(5); level->playSound(this, "random.eat", 0.5f, 1.0f); }
	}

	// sand or gravel fell on me: dig out, like anyone would
	if (isInWall()) {
		const int fx = Mth::floor(x), fz = Mth::floor(z), ey = Mth::floor(y), fy = Mth::floor(y - heightOffset);
		const int ty = level->getMaterial(fx, ey, fz)->blocksMotion() ? ey : fy;
		if (steps.empty() || steps.front().kind != Step::BREAK || steps.front().x != fx || steps.front().y != ty || steps.front().z != fz) {
			Step b; b.kind = Step::BREAK; b.x = fx; b.y = ty; b.z = fz; b.tile = -1; b.data = 0; b.ticks = 0;
			steps.push_front(b);
		}
	}

	// lost or stuck in a job for too long: rethink
	if (jobTicks > 20 * 180) { steps.clear(); job = JOB_NONE; }
	if (steps.empty()) think();
	static const bool log = getenv("MCPE_NPCLOG") != NULL; // development
	if (log && !steps.empty() && (tickCount % 400) == 0) {
		const Step& st = steps.front();
		printf("[npc] %s   step kind=%d at %d,%d,%d tile=%d (%d left) pos %.1f,%.1f,%.1f fails=%d direct=%d\n", getNpcName().c_str(), st.kind, st.x, st.y, st.z, st.tile,
			(int)steps.size(), x, y - heightOffset, z, routeFails, directTicks);
		fflush(stdout);
	}
	if (!steps.empty()) runStep();
	else held = ItemInstance();

	// swim, and never stand in place underwater
	if (isInWater() || isInLava()) jumping = true;
}

void PlayerNPC::push(Step::Kind k, int x, int y, int z, int tile, int data, int ticks)
{
	Step s;
	s.kind = k; s.x = x; s.y = y; s.z = z; s.tile = tile; s.data = data; s.ticks = ticks;
	steps.push_back(s);
}

void PlayerNPC::startJob(int j)
{
	job = j;
	jobTicks = 0;
	static const bool log = getenv("MCPE_NPCLOG") != NULL; // development
	if (log && (j != lastLoggedJob || level->getTime() - lastLogTime > 200)) {
		lastLoggedJob = j; lastLogTime = (int)level->getTime();
		static const char* names[] = { "none", "wood", "mine", "seeds", "craft", "table", "build", "help", "farm", "harvest", "pen", "extra", "night", "wander", "fight", "water" };
		printf("[npc] %s t=%ld job=%s at %d,%d,%d log=%d plank=%d stick=%d cob=%d seed=%d fence=%d gate=%d door=%d tools a%d p%d h%d s%d table=%d house=%d(%u/%u) farm=%d pen=%d\n",
			getNpcName().c_str(), (long)level->getTime(), names[j], Mth::floor(x), Mth::floor(y), Mth::floor(z),
			inv[I_LOG], inv[I_PLANK], inv[I_STICK], inv[I_COBBLE], inv[I_SEEDS], inv[I_FENCE], inv[I_GATE], inv[I_DOOR],
			axe, pick, hoe, sword, hasTable, houseDone, planPos, (unsigned)plan.size(), farmDone, penDone);
		fflush(stdout);
	}
}

// ---------------------------------------------------------------------------
// planner: what to do next
// ---------------------------------------------------------------------------
void PlayerNPC::think()
{
	digQueue.clear();
	// the previous job ran out of steps
	if ((job == JOB_BUILD || job == JOB_EXTRA) && !plan.empty() && planPos >= plan.size()) {
		// keep going: planBuild takes a second pass, then finishes
		if (planBuild(job == JOB_EXTRA)) return;
	}
	if (job == JOB_FARM) farmDone = true;
	if (job == JOB_PEN) penDone = true;
	if (job == JOB_NIGHT && level->isDay()) say(SAY_LINES[T_MORNING][random.nextInt(6)], T_MORNING);
	const int prevJob = job;
	job = JOB_NONE;

	if (fightTarget) {
		Entity* t = level->getEntity(fightTarget);
		if (t && t->isAlive() && t->distanceToSqr(this) < 16 * 16) { startJob(JOB_FIGHT); push(Step::LEAD, 0, 0, 0, 0, fightTarget, 20 * 20); return; }
		fightTarget = 0;
	}
	if (!hasHome) chooseHome();
	// down in my mine and not mining: walk back up the stairs first
	if (hasMine && (prevJob != JOB_MINE || inv[I_COBBLE] >= 48 || random.nextInt(4) == 0)) {
		const int fx = Mth::floor(x), fz = Mth::floor(z), fy = Mth::floor(y - heightOffset);
		if (fy < mineY - 3 && std::abs(fx - mineX) + std::abs(fz - mineZ) < 40) {
			startJob(JOB_WANDER);
			push(Step::MOVE, mineX, mineY, mineZ, 0, 0, 2);
			return;
		}
	}
	if (!level->isDay() && houseDone && planNight()) return;

	// a crafting table first, then wooden tools
	if (!hasTable) { if (inv[I_PLANK] + inv[I_LOG] * 4 >= 4) { if (planTable()) return; } if (planWood()) return; }
	if (axe == 0 || pick == 0 || (pick == 1 && inv[I_COBBLE] >= 5) || (sword == 0 && inv[I_PLANK] >= 3)) {
		if (inv[I_PLANK] + inv[I_LOG] * 4 >= 8 || (pick == 1 && inv[I_COBBLE] >= 5)) { if (planCraft()) return; }
		if (planWood()) return;
	}

	// my house (or help a neighbour now and then)
	if (!houseDone) {
		if (plan.empty()) makeHouseDesign(random.nextInt());
		if (random.nextInt(6) == 0 && planHelp()) return;
		int needP = 0, needC = 0, needL = 0;
		for (unsigned int i = planPos; i < plan.size(); ++i) {
			const Planned& p = plan[i];
			if (p.data == -2) continue; // optional
			if (p.tile == Tile::wood->id) needP++;
			else if (p.tile == Tile::stoneBrick->id) needC++;
			else if (p.tile == Tile::treeTrunk->id) needL++;
		}
		needP = Mth::Min(needP, 32); needC = Mth::Min(needC, 24); needL = Mth::Min(needL, 8); // build in parts
		keepLogs = needL;
		if (inv[I_PLANK] < needP && inv[I_LOG] > needL) { if (planCraft()) return; }
		if (inv[I_DOOR] == 0 && inv[I_PLANK] >= 6) { if (planCraft()) return; }
		if (inv[I_PLANK] + Mth::Max(0, inv[I_LOG] - needL) * 4 < needP || inv[I_LOG] < needL) { if (planWood()) return; }
		if (inv[I_COBBLE] < needC) { if (planMine()) return; }
		if (planBuild(false)) return;
	}

	// food
	if (farmDone && random.nextInt(3) == 0 && planHarvest()) return;
	if (!farmDone) {
		// seeds from tall grass if there is some (tilling grass turns some up too)
		if (inv[I_SEEDS] < 6 && seedTries < 3) { seedTries++; if (planSeeds()) return; }
		if (hoe == 0 || inv[I_FENCE] < 28 || inv[I_GATE] < 1) {
			if (inv[I_PLANK] + inv[I_LOG] * 4 >= 8) { if (planCraft()) return; }
			if (planWood()) return;
		} else if (planFarm()) return;
	}
	// animals
	if (!penDone) {
		if (inv[I_FENCE] < 26 || inv[I_GATE] < 1) {
			if (inv[I_PLANK] + inv[I_LOG] * 4 >= 8) { if (planCraft()) return; }
			if (planWood()) return;
		} else if (planPen()) return;
	}

	// after that: build extra things, help, keep collecting, look around
	const int r = random.nextInt(10);
	if (r < 3 && extrasBuilt < 4) {
		if (plan.empty()) makeExtraDesign(random.nextInt());
		if (afford(16, 12, 4) && planBuild(true)) return;
		plan.clear();
	}
	if (r < 5 && planHelp()) return;
	if (r < 6 && inv[I_LOG] < 16 && planWood()) return;
	if (r < 7 && inv[I_COBBLE] < 48 && planMine()) return;
	if (r < 8 && inv[I_LOG] > 0 && planCraft()) return;
	planWander();
}

void PlayerNPC::chooseHome()
{
	// next to the others (a little village), else where it spawned
	std::vector<PlayerNPC*> o = others();
	int cx = Mth::floor(x), cz = Mth::floor(z);
	for (unsigned int i = 0; i < o.size(); ++i) if (o[i]->hasHome) { cx = o[i]->homeX; cz = o[i]->homeZ; break; }
	for (int tries = 0; tries < 30; ++tries) {
		const float ang = random.nextFloat() * 6.2831853f;
		const int dist = (o.empty() ? 0 : 14) + random.nextInt(8);
		const int hx = cx + (int)(Mth::cos(ang) * dist), hz = cz + (int)(Mth::sin(ang) * dist);
		if (hx < 12 || hz < 12 || hx >= LEVEL_WIDTH - 12 || hz >= LEVEL_DEPTH - 12) continue;
		if (reserved(hx, hz, 9)) continue;
		const int gy = groundY(level, hx, hz);
		if (level->getMaterial(hx, gy - 1, hz)->isLiquid() || level->getMaterial(hx, gy, hz)->isLiquid()) continue;
		homeX = hx; homeY = gy; homeZ = hz;
		hasHome = true;
		return;
	}
	homeX = Mth::floor(x); homeY = Mth::floor(y - heightOffset); homeZ = Mth::floor(z);
	hasHome = true;
}

// Places other NPCs are building / farming on (and mine): don't dig or build there
bool PlayerNPC::reserved(int px, int pz, int margin) const
{
	std::vector<PlayerNPC*> all = const_cast<PlayerNPC*>(this)->others();
	all.push_back(const_cast<PlayerNPC*>(this));
	for (unsigned int i = 0; i < all.size(); ++i) {
		const PlayerNPC* n = all[i];
		if (n->hasHome && n != this && std::abs(n->homeX - px) < margin && std::abs(n->homeZ - pz) < margin) return true;
		if (n->planW > 0 && px >= n->planX - margin && px < n->planX + n->planW + margin && pz >= n->planZ - margin && pz < n->planZ + n->planD + margin) return true;
		if (n->farmW > 0 && px >= n->farmX - margin && px < n->farmX + n->farmW + margin && pz >= n->farmZ - margin && pz < n->farmZ + n->farmD + margin) return true;
		if (n->penW > 0 && px >= n->penX - margin && px < n->penX + n->penW + margin && pz >= n->penZ - margin && pz < n->penZ + n->penD + margin) return true;
	}
	return false;
}

bool PlayerNPC::planWood()
{
	if (inv[I_LOG] >= 64) return false; // plenty

	// nearest tree trunk, ring by ring outwards (looking under the leaves)
	int best = -1, bx = 0, by = 0, bz = 0;
	const int px = Mth::floor(x), pz = Mth::floor(z);
	for (int r = 1; r <= 28 && (best < 0 || r * r < best); ++r)
		for (int dx = -r; dx <= r; ++dx) for (int dz = -r; dz <= r; ++dz) {
			if (std::abs(dx) != r && std::abs(dz) != r) continue;
			const int cx = px + dx, cz = pz + dz;
			if (cx < 1 || cz < 1 || cx >= LEVEL_WIDTH - 1 || cz >= LEVEL_DEPTH - 1 || !level->isChunkLoaded(cx >> 4, cz >> 4)) continue;
			const int top = level->getHeightmap(cx, cz);
			for (int y = top; y > top - 12 && y > 1; --y) {
				if (!Tile::isLog(level->getTile(cx, y, cz))) continue;
				int base = y;
				while (base > 1 && Tile::isLog(level->getTile(cx, base - 1, cz))) base--;
				if (!level->getMaterial(cx, base - 1, cz)->blocksMotion()) break; // a branch, not a trunk
				if (reserved(cx, cz, 2) || isBad(cx, base, cz)) break;
				const int d = dx * dx + dz * dz;
				if (best < 0 || d < best) { best = d; bx = cx; by = base; bz = cz; }
				break;
			}
		}
	if (best < 0) return planExplore();
	exploreCount = 0;
	startJob(JOB_WOOD);
	if (lastTopicSaid != T_WOOD && random.nextInt(4) == 0) say(SAY_LINES[T_WOOD][random.nextInt(6)], T_WOOD);
	for (int y = by; y < by + 7; ++y) push(Step::BREAK, bx, y, bz, -1);
	return true;
}

// nothing useful around: head off in one direction for a while, like a player looking for trees
bool PlayerNPC::planExplore()
{
	if (exploreCount >= 8) { if (random.nextInt(400) == 0) exploreCount = 0; return false; }
	if (exploreCount == 0) exploreAngle = random.nextFloat() * 6.2831853f;
	exploreAngle += (random.nextFloat() - 0.5f) * 0.8f;
	exploreCount++;
	const int tx = Mth::floor(x + Mth::cos(exploreAngle) * 24), tz = Mth::floor(z + Mth::sin(exploreAngle) * 24);
	if (tx < 8 || tz < 8 || tx >= LEVEL_WIDTH - 8 || tz >= LEVEL_DEPTH - 8 || !level->isChunkLoaded(tx >> 4, tz >> 4)) {
		exploreAngle += 3.14159f;
		return false;
	}
	startJob(JOB_WANDER);
	if (exploreCount == 2 && random.nextInt(3) == 0) say(random.nextInt(2) ? "where are the trees" : "exploring a bit", T_BANTER);
	push(Step::MOVE, tx, groundY(level, tx, tz), tz, 0, 0, 3);
	return true;
}

bool PlayerNPC::planSeeds()
{
	const int px = Mth::floor(x), pz = Mth::floor(z);
	int found = 0;
	for (int i = 0; i < 200 && found < 10; ++i) {
		const int cx = px + random.nextInt(41) - 20, cz = pz + random.nextInt(41) - 20;
		if (cx < 1 || cz < 1 || cx >= LEVEL_WIDTH - 1 || cz >= LEVEL_DEPTH - 1 || !level->isChunkLoaded(cx >> 4, cz >> 4)) continue;
		const int gy = groundY(level, cx, cz);
		if (level->getTile(cx, gy, cz) == Tile::tallgrass->id) { push(Step::BREAK, cx, gy, cz, Tile::tallgrass->id); found++; }
	}
	if (!found) return false;
	startJob(JOB_SEEDS);
	return true;
}

bool PlayerNPC::planTable()
{
	if (!hasHome) chooseHome();
	startJob(JOB_TABLE);
	if (inv[I_PLANK] < 4) push(Step::CRAFT, 0, 0, 0, 0, 0, 20);
	// put it down right here, like anyone would: a free spot on the ground next to me
	const int px = Mth::floor(x), py = Mth::floor(y - heightOffset), pz = Mth::floor(z);
	int tx = px + 1, ty = py, tz = pz;
	bool found = false;
	for (int r = 1; r <= 3 && !found; ++r)
		for (int dx = -r; dx <= r && !found; ++dx) for (int dz = -r; dz <= r && !found; ++dz) {
			if (std::abs(dx) != r && std::abs(dz) != r) continue;
			for (int dy = -1; dy <= 1 && !found; ++dy) {
				const int cx = px + dx, cy = py + dy, cz = pz + dz;
				if (level->getTile(cx, cy, cz) != 0 || !level->getMaterial(cx, cy - 1, cz)->blocksMotion()) continue;
				if (level->getMaterial(cx, cy - 1, cz)->isLiquid() || reserved(cx, cz, 0) || isBad(cx, cy, cz)) continue;
				tx = cx; ty = cy; tz = cz; found = true;
			}
		}
	tableX = tx; tableY = ty; tableZ = tz;
	push(Step::PLACE, tx, ty, tz, Tile::workBench->id, 0);
	if (random.nextInt(3) == 0) say(SAY_LINES[T_CRAFT][random.nextInt(6)], T_CRAFT);
	return true;
}

void PlayerNPC::markBad(int bx, int by, int bz)
{
	static const bool log = getenv("MCPE_NPCLOG") != NULL;
	if (log) { printf("[npc] %s can't reach %d,%d,%d (job %d)\n", getNpcName().c_str(), bx, by, bz, job); fflush(stdout); }
	if (job == JOB_WOOD) {
		// the whole trunk: remember its base
		while (by > 1 && Tile::isLog(level->getTile(bx, by - 1, bz))) by--;
	}
	Bad b = { bx, by, bz, (int)level->getTime() };
	if (badSpots.size() >= 32) badSpots.erase(badSpots.begin());
	badSpots.push_back(b);
}

bool PlayerNPC::isBad(int bx, int by, int bz) const
{
	const int now = (int)level->getTime();
	for (unsigned int i = 0; i < badSpots.size(); ++i) {
		const Bad& b = badSpots[i];
		if (b.x == bx && b.z == bz && std::abs(b.y - by) <= 1 && now - b.time < 20 * 60 * 5) return true;
	}
	return false;
}

bool PlayerNPC::planCraft()
{
	startJob(JOB_CRAFT);
	if (hasTable && level->getTile(tableX, tableY, tableZ) != Tile::workBench->id) hasTable = false;
	// planks, sticks and a table come out of the little 2x2 grid; the rest needs the table
	const bool atTable = hasTable && wantTable();
	if (atTable) push(Step::MOVE, tableX, tableY, tableZ, 0, 0, 3);
	push(Step::CRAFT, 0, 0, 0, 0, atTable ? 1 : 0, (atTable ? 40 : 20) + random.nextInt(40));
	return true;
}

bool PlayerNPC::wantTable() const
{
	if (axe == 0 || pick == 0 || sword == 0) return true;
	if ((axe < 2 || pick < 2 || sword < 2) && inv[I_COBBLE] >= 3) return true;
	if (inv[I_DOOR] == 0 && inv[I_PLANK] >= 6) return true;
	if (houseDone && hoe == 0) return true;
	if (houseDone && (!farmDone || !penDone) && (inv[I_FENCE] < 56 || inv[I_GATE] < 2) && inv[I_PLANK] >= 4) return true;
	if (inv[I_COAL] > 0 && inv[I_TORCH] < 16) return true;
	if (inv[I_FURNACE] == 0 && inv[I_COBBLE] >= 16) return true;
	if (inv[I_WHEAT] >= 3) return true;
	return false;
}

void PlayerNPC::craftAll(bool atTable)
{
	// what a player would make right now, given what it wants next
	int wantPlanks = 64;
	if (inv[I_LOG] > 0 && inv[I_PLANK] < wantPlanks) {
		const int keep = houseDone ? 0 : keepLogs; // logs for the house's log blocks
		const int use = Mth::Max(0, Mth::Min(inv[I_LOG] - keep, (wantPlanks - inv[I_PLANK] + 3) / 4));
		inv[I_LOG] -= use; inv[I_PLANK] += use * 4;
		if (inv[I_PLANK] < 4 && inv[I_LOG] > 0) { inv[I_LOG]--; inv[I_PLANK] += 4; }
	}
	if (!hasTable && inv[I_PLANK] >= 4 && inv[I_TABLE] == 0) { inv[I_PLANK] -= 4; inv[I_TABLE]++; }
	// a table for the house when the first one ended up far away
	if (inv[I_TABLE] == 0 && !houseDone && planW > 0 && hasTable && inv[I_PLANK] >= 12
		&& std::abs(tableX - homeX) + std::abs(tableZ - homeZ) > 12) { inv[I_PLANK] -= 4; inv[I_TABLE]++; }
	while (inv[I_STICK] < 8 && inv[I_PLANK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
	if (!atTable) return;
	if (axe == 0 && inv[I_PLANK] >= 3 && inv[I_STICK] >= 2) { inv[I_PLANK] -= 3; inv[I_STICK] -= 2; axe = 1; }
	if (pick == 0 && inv[I_PLANK] >= 3 && inv[I_STICK] >= 2) { inv[I_PLANK] -= 3; inv[I_STICK] -= 2; pick = 1; }
	if (sword == 0 && inv[I_PLANK] >= 2 && inv[I_STICK] >= 1) { inv[I_PLANK] -= 2; inv[I_STICK] -= 1; sword = 1; }
	bool upgraded = false;
	if (pick < 2 && inv[I_COBBLE] >= 3 && inv[I_STICK] >= 2) { inv[I_COBBLE] -= 3; inv[I_STICK] -= 2; pick = 2; upgraded = true; }
	if (axe < 2 && inv[I_COBBLE] >= 3 && inv[I_STICK] >= 2) { inv[I_COBBLE] -= 3; inv[I_STICK] -= 2; axe = 2; upgraded = true; }
	if (sword < 2 && inv[I_COBBLE] >= 2 && inv[I_STICK] >= 1) { inv[I_COBBLE] -= 2; inv[I_STICK] -= 1; sword = 2; }
	if (upgraded && random.nextInt(2) == 0) say(SAY_LINES[T_TOOLS][random.nextInt(6)], T_TOOLS);
	if (hoe == 0 && houseDone && inv[I_PLANK] >= 2 && inv[I_STICK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] -= 2; hoe = 1; }
	if (inv[I_DOOR] == 0 && inv[I_PLANK] >= 6) { inv[I_PLANK] -= 6; inv[I_DOOR] += 3; }
	if (houseDone && (!farmDone || !penDone)) {
		while (inv[I_GATE] < 2 && inv[I_PLANK] >= 2) {
			while (inv[I_STICK] < 4 && inv[I_PLANK] >= 4) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
			if (inv[I_STICK] < 4) break;
			inv[I_PLANK] -= 2; inv[I_STICK] -= 4; inv[I_GATE]++;
		}
		while (inv[I_FENCE] < 56 && inv[I_PLANK] >= 4 && inv[I_STICK] >= 2) {
			inv[I_PLANK] -= 4; inv[I_STICK] -= 2; inv[I_FENCE] += 3;
			while (inv[I_STICK] < 2 && inv[I_PLANK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
		}
		while (inv[I_GATE] < 2 && inv[I_PLANK] >= 2 && inv[I_STICK] >= 4) { inv[I_PLANK] -= 2; inv[I_STICK] -= 4; inv[I_GATE]++; }
	}
	while (inv[I_COAL] > 0 && inv[I_STICK] > 0 && inv[I_TORCH] < 16) { inv[I_COAL]--; inv[I_STICK]--; inv[I_TORCH] += 4; }
	if (inv[I_FURNACE] == 0 && inv[I_COBBLE] >= 16) { inv[I_COBBLE] -= 8; inv[I_FURNACE]++; }
	while (inv[I_WHEAT] >= 3) { inv[I_WHEAT] -= 3; inv[I_BREAD]++; }
}

bool PlayerNPC::afford(int planks, int cobble, int logs)
{
	return inv[I_PLANK] >= planks && inv[I_COBBLE] >= cobble && inv[I_LOG] >= logs;
}

bool PlayerNPC::planMine()
{
	if (pick == 0) return false;
	if (!hasMine || mineY - mineDepth < 14 || mineFails > 2) {
		hasMine = false;
		// a new staircase near home, away from everyone's buildings
		for (int tries = 0; tries < 20; ++tries) {
			const float ang = random.nextFloat() * 6.2831853f;
			const int d = 8 + random.nextInt(6);
			const int mx = homeX + (int)(Mth::cos(ang) * d), mz = homeZ + (int)(Mth::sin(ang) * d);
			const int dir = random.nextInt(4);
			const int ex = mx + (dir == 0 ? 12 : dir == 1 ? -12 : 0), ez = mz + (dir == 2 ? 12 : dir == 3 ? -12 : 0);
			if (mx < 16 || mz < 16 || mx >= LEVEL_WIDTH - 16 || mz >= LEVEL_DEPTH - 16) continue;
			if (ex < 16 || ez < 16 || ex >= LEVEL_WIDTH - 16 || ez >= LEVEL_DEPTH - 16) continue;
			if (reserved(mx, mz, 3) || reserved(ex, ez, 3)) continue;
			mineX = mx; mineZ = mz; mineDir = dir; mineDepth = 0; mineFails = 0;
			mineY = groundY(level, mx, mz);
			hasMine = true;
			break;
		}
		if (!hasMine) return false;
	}
	startJob(JOB_MINE);
	if (lastTopicSaid != T_STONE && random.nextInt(3) == 0) say(SAY_LINES[T_STONE][random.nextInt(6)], T_STONE);
	const int dx = mineDir == 0 ? 1 : mineDir == 1 ? -1 : 0, dz = mineDir == 2 ? 1 : mineDir == 3 ? -1 : 0;
	for (int i = 0; i < 6; ++i) {
		const int k = mineDepth + 1 + i;
		const int cx = mineX + dx * k, cz = mineZ + dz * k, fy = mineY - k;
		if (fy < 12) break;
		push(Step::BREAK, cx, fy + 2, cz, -1);
		push(Step::BREAK, cx, fy + 1, cz, -1);
		push(Step::BREAK, cx, fy, cz, -1);
		push(Step::MOVE, cx, fy, cz, k, 1, 1); // data 1: mining step (look for ore around); tile: depth
	}
	return true;
}

bool PlayerNPC::planNight()
{
	if (!houseDone || planW <= 0) return false;
	startJob(JOB_NIGHT);
	if (random.nextInt(2) == 0) say(SAY_LINES[T_NIGHT][random.nextInt(6)], T_NIGHT);
	push(Step::MOVE, planX + planW / 2, planY, planZ + planD / 2, 0, 0, 1);
	push(Step::WAIT, 0, 0, 0, 0, 0, 20 * 30);
	return true;
}

void PlayerNPC::planWander()
{
	startJob(JOB_WANDER);
	const int cx = (hasHome ? homeX : Mth::floor(x)) + random.nextInt(17) - 8;
	const int cz = (hasHome ? homeZ : Mth::floor(z)) + random.nextInt(17) - 8;
	if (cx > 2 && cz > 2 && cx < LEVEL_WIDTH - 2 && cz < LEVEL_DEPTH - 2)
		push(Step::MOVE, cx, groundY(level, cx, cz), cz, 0, 0, 2);
	push(Step::WAIT, 0, 0, 0, 0, 2, 40 + random.nextInt(100)); // data 2: look around
}

// ---------------------------------------------------------------------------
// building: each house is generated from a seed (saved), so every one differs
// ---------------------------------------------------------------------------
void PlayerNPC::makeHouseDesign(int seed)
{
	plan.clear();
	planPos = 0;
	planSeed = seed;
	Random r(seed);
	const int w = 5 + r.nextInt(3), d = 5 + r.nextInt(3), h = 3 + r.nextInt(2);
	if (planW == 0) {
		// the house goes on the home spot
		planX = homeX - w / 2; planZ = homeZ - d / 2;
		planY = homeY;
	}
	planW = w; planD = d;
	const int P = Tile::wood->id, C = Tile::stoneBrick->id, L = Tile::treeTrunk->id;
	const int wallStyle = r.nextInt(4);           // planks, cobble, cobble base, logs
	const bool logCorners = r.nextInt(10) < 7;
	const int floorT = r.nextInt(2) ? P : C;
	const int roofStyle = r.nextInt(3);           // flat, pyramid, gable
	const int windows = r.nextInt(3);              // none, holes, fence bars
	const int plankData = r.nextInt(3);            // oak, spruce, birch planks
	const int doorSide = r.nextInt(4);
	const int x0 = planX, y0 = planY, z0 = planZ;
	Planned p;
#define ADD(X, Y, Z, T, D) do { p.x = (X); p.y = (Y); p.z = (Z); p.tile = (T); p.data = (D); plan.push_back(p); } while (0)
	// clear the inside and fill holes under the floor
	for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) {
		ADD(x0 + xx, y0 - 2, z0 + zz, Tile::dirt->id, -1); // only where there's nothing
		for (int yy = h + 4; yy >= 0; --yy) ADD(x0 + xx, y0 + yy, z0 + zz, 0, 0);
	}
	for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) ADD(x0 + xx, y0 - 1, z0 + zz, floorT, floorT == P ? plankData : 0);
	// door position
	int doorX = x0 + w / 2, doorZ = z0, doorData = 1;
	if (doorSide == 1) { doorZ = z0 + d - 1; doorData = 3; }
	if (doorSide == 2) { doorX = x0; doorZ = z0 + d / 2; doorData = 0; }
	if (doorSide == 3) { doorX = x0 + w - 1; doorZ = z0 + d / 2; doorData = 2; }
	// walls, bottom to top
	for (int yy = 0; yy < h; ++yy)
		for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) {
			const bool edgeX = xx == 0 || xx == w - 1, edgeZ = zz == 0 || zz == d - 1;
			if (!edgeX && !edgeZ) continue;
			const int wx = x0 + xx, wz = z0 + zz;
			if (wx == doorX && wz == doorZ && yy < 2) continue;
			const bool corner = edgeX && edgeZ;
			// windows in the middle of each wall, one block up
			const bool mid = (edgeZ && !edgeX && (xx == w / 2 || xx == w / 2 - (w % 2 == 0))) || (edgeX && !edgeZ && (zz == d / 2 || zz == d / 2 - (d % 2 == 0)));
			if (windows && yy == 1 && mid && !(wx == doorX || wz == doorZ)) {
				if (windows == 2) ADD(wx, y0 + yy, wz, Tile::fence->id, -2);
				continue;
			}
			int t = P, dt = plankData;
			if (corner && logCorners) { t = L; dt = 0; }
			else if (wallStyle == 1) { t = C; dt = 0; }
			else if (wallStyle == 2 && yy == 0) { t = C; dt = 0; }
			else if (wallStyle == 3) { t = L; dt = 0; }
			ADD(wx, y0 + yy, wz, t, dt);
		}
	// roof
	if (roofStyle == 0) {
		for (int xx = -1; xx <= w; ++xx) for (int zz = -1; zz <= d; ++zz) {
			const bool rim = xx == -1 || xx == w || zz == -1 || zz == d;
			ADD(x0 + xx, y0 + h, z0 + zz, rim && logCorners ? L : P, rim && logCorners ? 0 : plankData);
		}
	} else if (roofStyle == 1) {
		for (int k = 0; ; ++k) {
			const int ax = -1 + k, bx = w - k, az = -1 + k, bz = d - k;
			if (ax > bx || az > bz) break;
			for (int xx = ax; xx <= bx; ++xx) for (int zz = az; zz <= bz; ++zz) {
				const bool ring = xx == ax || xx == bx || zz == az || zz == bz;
				if (ring || ax + 1 >= bx || az + 1 >= bz) ADD(x0 + xx, y0 + h + k, z0 + zz, P, plankData);
			}
		}
	} else {
		const bool alongX = w >= d;
		const int span = alongX ? d : w, len = alongX ? w : d;
		for (int k = 0; k <= (span + 1) / 2; ++k) {
			const int a = -1 + k, b = span - k;
			if (a > b) break;
			for (int l = -1; l <= len; ++l) {
				for (int s = a; s <= b; ++s) {
					const bool slope = s == a || s == b;
					const bool gableEnd = (l == 0 || l == len - 1);
					if (!slope && !gableEnd) continue;
					if (!slope && (l == -1 || l == len)) continue;
					const int xx = alongX ? l : s, zz = alongX ? s : l;
					ADD(x0 + xx, y0 + h + k, z0 + zz, slope ? (r.nextInt(5) ? P : L) : P, plankData);
				}
			}
		}
	}
	// door (placed open: they never close doors) and lights
	ADD(doorX, y0, doorZ, Tile::door_wood->id, doorData | 4);
	ADD(x0 + 1, y0, z0 + 1, Tile::torch->id, -2);
	ADD(x0 + w - 2, y0, z0 + d - 2, Tile::torch->id, -2);
	if (doorSide < 2) ADD(doorX + 1, y0 + 2, doorZ + (doorSide == 0 ? -1 : 1), Tile::torch->id, -2);
	ADD(x0 + w - 2, y0, z0 + 1, Tile::workBench->id, -2);
	ADD(x0 + 1, y0, z0 + d - 2, Tile::furnace->id, -2);
#undef ADD
}

void PlayerNPC::makeExtraDesign(int seed)
{
	plan.clear();
	planPos = 0;
	planSeed = seed ^ 0x5a5a;
	Random r(seed);
	const int kind = r.nextInt(4);
	const int P = Tile::wood->id, C = Tile::stoneBrick->id, L = Tile::treeTrunk->id, F = Tile::fence->id;
	// somewhere near home that's free
	int ox = homeX, oz = homeZ;
	for (int tries = 0; tries < 20; ++tries) {
		const float ang = r.nextFloat() * 6.2831853f;
		const int dd = 9 + r.nextInt(8);
		ox = homeX + (int)(Mth::cos(ang) * dd); oz = homeZ + (int)(Mth::sin(ang) * dd);
		if (ox > 10 && oz > 10 && ox < LEVEL_WIDTH - 10 && oz < LEVEL_DEPTH - 10 && !reserved(ox, oz, 4)) break;
	}
	const int oy = groundY(level, ox, oz);
	Planned p;
#define ADD(X, Y, Z, T, D) do { p.x = (X); p.y = (Y); p.z = (Z); p.tile = (T); p.data = (D); plan.push_back(p); } while (0)
	if (kind == 0) {
		// lookout tower: log pillars, platform, railing
		const int hgt = 5 + r.nextInt(3);
		planX = ox; planZ = oz; planW = 3; planD = 3; planY = oy;
		for (int yy = 0; yy < hgt; ++yy) { ADD(ox, oy + yy, oz, L, 0); ADD(ox + 2, oy + yy, oz, L, 0); ADD(ox, oy + yy, oz + 2, L, 0); ADD(ox + 2, oy + yy, oz + 2, L, 0); }
		for (int xx = -1; xx <= 3; ++xx) for (int zz = -1; zz <= 3; ++zz) ADD(ox + xx, oy + hgt, oz + zz, P, 0);
		for (int xx = -1; xx <= 3; ++xx) for (int zz = -1; zz <= 3; ++zz)
			if (xx == -1 || xx == 3 || zz == -1 || zz == 3) ADD(ox + xx, oy + hgt + 1, oz + zz, F, -2);
		ADD(ox + 1, oy + hgt + 1, oz + 1, Tile::torch->id, -2);
	} else if (kind == 1) {
		// lamp posts around home
		planW = 0;
		for (int i = 0; i < 4; ++i) {
			const int lx = homeX + (i & 1 ? 7 : -7), lz = homeZ + (i & 2 ? 7 : -7);
			const int ly = groundY(level, lx, lz);
			ADD(lx, ly, lz, F, 0); ADD(lx, ly + 1, lz, F, 0); ADD(lx, ly + 2, lz, Tile::torch->id, -2);
		}
	} else if (kind == 2) {
		// small storage hut
		const int w = 3 + r.nextInt(2), d = 3 + r.nextInt(2);
		planX = ox; planZ = oz; planW = w; planD = d; planY = oy;
		for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) {
			for (int yy = 3; yy >= 0; --yy) ADD(ox + xx, oy + yy, oz + zz, 0, 0);
			ADD(ox + xx, oy - 1, oz + zz, C, 0);
		}
		for (int yy = 0; yy < 3; ++yy) for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) {
			if (xx != 0 && xx != w - 1 && zz != 0 && zz != d - 1) continue;
			if (xx == w / 2 && zz == 0 && yy < 2) continue;
			ADD(ox + xx, oy + yy, oz + zz, yy == 2 ? P : C, 0);
		}
		for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) ADD(ox + xx, oy + 3, oz + zz, P, 0);
		ADD(ox + w / 2, oy, oz, Tile::door_wood->id, 1 | 4);
	} else {
		// cobble path from home outwards
		planW = 0;
		const int dir = r.nextInt(4);
		const int dx = dir == 0 ? 1 : dir == 1 ? -1 : 0, dz = dir == 2 ? 1 : dir == 3 ? -1 : 0;
		for (int i = 4; i < 14; ++i) {
			const int px = homeX + dx * i, pz = homeZ + dz * i;
			if (px < 2 || pz < 2 || px >= LEVEL_WIDTH - 2 || pz >= LEVEL_DEPTH - 2) break;
			const int py = groundY(level, px, pz);
			if (reserved(px, pz, 0) && !(std::abs(px - homeX) < 5 && std::abs(pz - homeZ) < 5)) break;
			ADD(px, py - 1, pz, C, 0);
			if (i % 5 == 0) { ADD(px + dz, py, pz + dx, F, 0); ADD(px + dz, py + 1, pz + dx, Tile::torch->id, -2); }
		}
	}
#undef ADD
}

bool PlayerNPC::planBuild(bool extra)
{
	if (plan.empty()) return false;
	if (planPos >= plan.size()) {
		// went round once: look again for anything skipped (unreachable then, maybe not now)
		if (buildPasses >= 1) return planFinish(extra);
		buildPasses++;
		planPos = 0;
	}
	startJob(extra ? JOB_EXTRA : JOB_BUILD);
	if (!extra && !announcedBuild) { announcedBuild = true; say(SAY_LINES[T_BUILD][random.nextInt(6)], T_BUILD); }
	if (fillBuildSteps(this, 40) == 0) {
		if (planPos >= plan.size() && buildPasses >= 1) { steps.clear(); return planFinish(extra); }
		return false;
	}
	return true;
}

// the plan is all done (or nothing left that can be reached)
bool PlayerNPC::planFinish(bool extra)
{
	plan.clear();
	planPos = 0;
	buildPasses = 0;
	if (extra) { extrasBuilt++; planW = 0; }
	else { houseDone = true; say(SAY_LINES[T_BUILT][random.nextInt(6)], T_BUILT); }
	job = JOB_NONE;
	return false;
}

// takes the next blocks of owner's plan and turns them into steps for me
int PlayerNPC::fillBuildSteps(PlayerNPC* owner, int maxBlocks)
{
	int n = 0;
	int bx, by, bz, t, d;
	while (n < maxBlocks) {
		const unsigned int idx = owner->planPos;
		if (!owner->claimBuildBlock(bx, by, bz, t, d)) break;
		if (t == 0) push(Step::BREAK, bx, by, bz, -1, 0, (int)idx);
		else push(Step::PLACE, bx, by, bz, t, d, (int)idx);
		n++;
	}
	return n;
}

// next unfinished block of my plan (helpers call this too)
bool PlayerNPC::claimBuildBlock(int& bx, int& by, int& bz, int& tile, int& data)
{
	while (planPos < plan.size()) {
		const Planned& p = plan[planPos++];
		const int cur = level->getTile(p.x, p.y, p.z);
		if (p.tile == 0) {
			if (cur == 0 || cur == Tile::workBench->id) continue;
			const Material* m = level->getMaterial(p.x, p.y, p.z);
			if (m->isLiquid()) continue;
			bx = p.x; by = p.y; bz = p.z; tile = 0; data = 0;
			return true;
		}
		if (cur == p.tile) continue;
		if (p.data == -1 && cur != 0 && !level->getMaterial(p.x, p.y, p.z)->isLiquid()) continue; // fill only
		bx = p.x; by = p.y; bz = p.z; tile = p.tile; data = p.data;
		return true;
	}
	return false;
}

bool PlayerNPC::planHelp()
{
	std::vector<PlayerNPC*> o = others();
	for (unsigned int i = 0; i < o.size(); ++i) {
		PlayerNPC* n = o[i];
		if (!n->isBuilding() || n->distanceToSqr(this) > 64 * 64 || n->planPos >= n->plan.size()) continue;
		if (inv[I_PLANK] < 8 && inv[I_COBBLE] < 8) continue;
		if (fillBuildSteps(n, 12) == 0) continue;
		helpingId = n->entityId;
		startJob(JOB_HELP);
		if (random.nextInt(2) == 0) say(SAY_LINES[T_HELP][random.nextInt(6)], T_HELP);
		return true;
	}
	return false;
}

bool PlayerNPC::planFarm()
{
	if (farmW == 0) {
		for (int tries = 0; tries < 30; ++tries) {
			const float ang = random.nextFloat() * 6.2831853f;
			const int dd = 9 + random.nextInt(7);
			const int w = 5 + random.nextInt(3), d = 5 + random.nextInt(3);
			const int fx = homeX + (int)(Mth::cos(ang) * dd) - w / 2, fz = homeZ + (int)(Mth::sin(ang) * dd) - d / 2;
			if (fx < 12 || fz < 12 || fx + w >= LEVEL_WIDTH - 12 || fz + d >= LEVEL_DEPTH - 12) continue;
			bool ok = true;
			for (int xx = -1; xx <= w && ok; ++xx) for (int zz = -1; zz <= d && ok; ++zz) if (reserved(fx + xx, fz + zz, 1)) ok = false;
			if (!ok) continue;
			// fairly flat, on dirt or grass
			const int cy = groundY(level, fx + w / 2, fz + d / 2);
			int bumps = 0;
			for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) {
				const int gy = groundY(level, fx + xx, fz + zz);
				if (std::abs(gy - cy) > 1) bumps++;
				if (level->getMaterial(fx + xx, gy - 1, fz + zz)->isLiquid()) bumps += 5;
			}
			if (bumps > 2) continue;
			farmX = fx; farmZ = fz; farmW = w; farmD = d; farmY = cy;
			break;
		}
		if (farmW == 0) return false;
	}
	startJob(JOB_FARM);
	say(SAY_LINES[T_FARM][random.nextInt(6)], T_FARM);
	// fence ring with a gate on the side facing home
	const int gateX = farmX + farmW / 2, gateZ = (homeZ < farmZ) ? farmZ - 1 : farmZ + farmD;
	for (int xx = -1; xx <= farmW; ++xx) for (int zz = -1; zz <= farmD; ++zz) {
		if (xx != -1 && xx != farmW && zz != -1 && zz != farmD) continue;
		const int fx = farmX + xx, fz = farmZ + zz;
		if (fx == gateX && fz == gateZ) push(Step::PLACE, fx, farmY, fz, Tile::fenceGate->id, 0);
		else push(Step::PLACE, fx, farmY, fz, Tile::fence->id, 0);
	}
	// water in the middle, then till and plant
	const int wx = farmX + farmW / 2, wz = farmZ + farmD / 2;
	if (!waterPlaced) push(Step::PLACE, wx, farmY - 1, wz, Tile::calmWater->id, 0);
	for (int xx = 0; xx < farmW; ++xx) for (int zz = 0; zz < farmD; ++zz) {
		const int fx = farmX + xx, fz = farmZ + zz;
		if (fx == wx && fz == wz) continue;
		push(Step::TILL, fx, farmY - 1, fz);
		push(Step::PLACE, fx, farmY, fz, Tile::crops->id, 0);
	}
	return true;
}

bool PlayerNPC::planHarvest()
{
	if (farmW == 0) return false;
	int ripe = 0;
	for (int xx = 0; xx < farmW; ++xx) for (int zz = 0; zz < farmD; ++zz) {
		const int fx = farmX + xx, fz = farmZ + zz;
		if (level->getTile(fx, farmY, fz) == Tile::crops->id && level->getData(fx, farmY, fz) >= 7) {
			push(Step::BREAK, fx, farmY, fz, Tile::crops->id);
			push(Step::PLACE, fx, farmY, fz, Tile::crops->id, 0);
			ripe++;
		} else if (level->getTile(fx, farmY, fz) == 0 && level->getTile(fx, farmY - 1, fz) == Tile::farmland->id && inv[I_SEEDS] > 0) {
			push(Step::PLACE, fx, farmY, fz, Tile::crops->id, 0);
		}
	}
	if (steps.empty()) return false;
	startJob(JOB_HARVEST);
	if (ripe > 0 && random.nextInt(2) == 0) say(SAY_LINES[T_HARVEST][random.nextInt(6)], T_HARVEST);
	if (hasTable && inv[I_WHEAT] + ripe >= 3) { push(Step::MOVE, tableX, tableY, tableZ, 0, 0, 3); push(Step::CRAFT, 0, 0, 0, 0, 1, 30); } // bread
	return true;
}

bool PlayerNPC::planPen()
{
	// animals around?
	std::vector<Entity*> animals;
	int counts[4] = { 0, 0, 0, 0 };
	const int types[4] = { MobTypes::Cow, MobTypes::Sheep, MobTypes::Pig, MobTypes::Chicken };
	for (unsigned int i = 0; i < level->entities.size(); ++i) {
		Entity* e = level->entities[i];
		if (e->removed || !e->isAlive() || e->distanceToSqr(this) > 48 * 48) continue;
		for (int t = 0; t < 4; ++t) if (e->getEntityTypeId() == types[t]) counts[t]++;
	}
	int best = 0;
	for (int t = 1; t < 4; ++t) if (counts[t] > counts[best]) best = t;
	if (counts[best] == 0) return false;

	if (penW == 0) {
		for (int tries = 0; tries < 30; ++tries) {
			const float ang = random.nextFloat() * 6.2831853f;
			const int dd = 10 + random.nextInt(8);
			const int w = 5 + random.nextInt(3), d = 5 + random.nextInt(2);
			const int px = homeX + (int)(Mth::cos(ang) * dd) - w / 2, pz = homeZ + (int)(Mth::sin(ang) * dd) - d / 2;
			if (px < 12 || pz < 12 || px + w >= LEVEL_WIDTH - 12 || pz + d >= LEVEL_DEPTH - 12) continue;
			bool ok = true;
			for (int xx = -1; xx <= w && ok; ++xx) for (int zz = -1; zz <= d && ok; ++zz) if (reserved(px + xx, pz + zz, 1)) ok = false;
			if (!ok) continue;
			const int cy = groundY(level, px + w / 2, pz + d / 2);
			int bumps = 0;
			for (int xx = -1; xx <= w; ++xx) for (int zz = -1; zz <= d; ++zz) {
				const int gy = groundY(level, px + xx, pz + zz);
				if (std::abs(gy - cy) > 1) bumps++;
				if (level->getMaterial(px + xx, gy - 1, pz + zz)->isLiquid()) bumps += 5;
			}
			if (bumps > 3) continue;
			penX = px; penZ = pz; penW = w; penD = d; penY = cy;
			break;
		}
		if (penW == 0) return false;
	}
	startJob(JOB_PEN);
	say(SAY_LINES[T_PEN][random.nextInt(6)], T_PEN);
	const int gateX = penX + penW / 2, gateZ = (homeZ < penZ) ? penZ - 1 : penZ + penD;
	for (int xx = -1; xx <= penW; ++xx) for (int zz = -1; zz <= penD; ++zz) {
		if (xx != -1 && xx != penW && zz != -1 && zz != penD) continue;
		const int fx = penX + xx, fz = penZ + zz;
		const int fy = groundY(level, fx, fz);
		if (fx == gateX && fz == gateZ) push(Step::PLACE, fx, fy, fz, Tile::fenceGate->id, 4); // open while leading
		else push(Step::PLACE, fx, fy, fz, Tile::fence->id, 0);
	}
	// bring in up to three of the most common animal
	int led = 0;
	for (unsigned int i = 0; i < level->entities.size() && led < 3; ++i) {
		Entity* e = level->entities[i];
		if (e->removed || !e->isAlive() || e->getEntityTypeId() != types[best] || e->distanceToSqr(this) > 48 * 48) continue;
		push(Step::LEAD, 0, 0, 0, 1, e->entityId, 20 * 40);
		led++;
	}
	// close the gate behind them
	push(Step::MOVE, gateX, groundY(level, gateX, gateZ), gateZ + (gateZ < penZ ? -2 : 2), 0, 0, 1);
	push(Step::PLACE, gateX, groundY(level, gateX, gateZ), gateZ, Tile::fenceGate->id, 0);
	return true;
}

// ---------------------------------------------------------------------------
// executor
// ---------------------------------------------------------------------------
// building things a bit taller than a player can reach without pillaring up
float PlayerNPC::reach() const
{
	return (job == JOB_BUILD || job == JOB_EXTRA || job == JOB_HELP) ? 5.4f : 4.4f;
}

bool PlayerNPC::inReach(int bx, int by, int bz) const
{
	const float dx = bx + 0.5f - x, dy = by + 0.5f - y, dz = bz + 0.5f - z;
	const float r = reach();
	return dx * dx + dy * dy + dz * dz <= r * r;
}

bool PlayerNPC::standable(int sx, int sy, int sz)
{
	if (sy < 1 || sy >= Level::DEPTH - 2) return false;
	if (!level->getMaterial(sx, sy - 1, sz)->blocksMotion()) return false;
	if (level->getMaterial(sx, sy, sz)->blocksMotion() || level->getMaterial(sx, sy + 1, sz)->blocksMotion()) return false;
	if (level->getMaterial(sx, sy, sz)->isLiquid() && level->getTile(sx, sy, sz) == Tile::lava->id) return false;
	return true;
}

bool PlayerNPC::standSpotFor(int bx, int by, int bz, int& sx, int& sy, int& sz)
{
	float best = 1e9f;
	bool found = false;
	for (int r = 1; r <= 4; ++r)
		for (int dx = -r; dx <= r; ++dx) for (int dz = -r; dz <= r; ++dz) {
			if (std::abs(dx) != r && std::abs(dz) != r) continue;
			for (int dy = -6; dy <= 2; ++dy) {
				const int cx = bx + dx, cy = by + dy, cz = bz + dz;
				if (cx == bx && cz == bz && (cy == by || cy + 1 == by)) continue; // standing in the block
				if (!standable(cx, cy, cz)) continue;
				const float ex = bx + 0.5f - (cx + 0.5f), ey = by + 0.5f - (cy + 1.62f), ez = bz + 0.5f - (cz + 0.5f);
				const float rr = reach() - 0.5f;
				bool skip = false;
				for (unsigned int k = 0; k < standExcluded.size(); ++k) if (standExcluded[k] == ((cx & 0x3ff) | ((cy & 0xff) << 10) | ((cz & 0x3ff) << 18))) skip = true;
				if (skip) continue;
				if (ex * ex + ey * ey + ez * ez > rr * rr) continue;
				const float d = (cx + 0.5f - x) * (cx + 0.5f - x) + (cz + 0.5f - z) * (cz + 0.5f - z) + (cy - (y - heightOffset)) * (cy - (y - heightOffset)) * 2;
				if (d < best) { best = d; sx = cx; sy = cy; sz = cz; found = true; }
			}
		}
	return found;
}

// 1 arrived, 0 walking, -1 gave up
bool PlayerNPC::walkTo(float tx, float ty, float tz, float reach)
{
	const float dx = tx + 0.5f - x, dz = tz + 0.5f - z, dy = ty - (y - heightOffset);
	const float d2 = dx * dx + dz * dz;
	if (d2 <= reach * reach && std::fabs(dy) < 1.6f) { route.destroy(); directTicks = 0; return true; }
	// no closer after a while: dig through what's in front (or give up eventually)
	if (progDist >= 1e8f) { progDist = Mth::sqrt(d2) + std::fabs(dy); progTimer = 0; }
	if (++progTimer >= 60) {
		progTimer = 0;
		const float dist = Mth::sqrt(d2) + std::fabs(dy);
		if (progDist - dist < 1.0f && progDist < 1e8f) {
			routeFails++;
			const float ang = (float)Mth::atan2(dz, dx);
			if (dy > 1.5f && (inv[I_DIRT] + inv[I_COBBLE] + inv[I_PLANK]) > 0) {
				// down in a hole: jump and put a block under me, like anyone would
				Step p; p.kind = Step::PILLAR; p.x = Mth::floor(x); p.y = Mth::floor(y - heightOffset); p.z = Mth::floor(z); p.tile = 0; p.data = 0; p.ticks = 0;
				digQueue.push_back(p);
			}
			const int fx = Mth::floor(x + Mth::cos(ang) * 0.9f), fz = Mth::floor(z + Mth::sin(ang) * 0.9f), fy = Mth::floor(y - heightOffset);
			for (int k = 1; k >= 0; --k) {
				const int t = level->getTile(fx, fy + k, fz);
				if (t != 0 && level->getMaterial(fx, fy + k, fz)->blocksMotion() && t != Tile::unbreakable->id && t != Tile::invisible_bedrock->id && t != Tile::chest->id) {
					Step b; b.kind = Step::BREAK; b.x = fx; b.y = fy + k; b.z = fz; b.tile = -1; b.data = 0; b.ticks = 0;
					digQueue.push_back(b);
				}
			}
			// a block over my head keeps me from jumping out
			const int up = level->getTile(Mth::floor(x), fy + 2, Mth::floor(z));
			if (up != 0 && level->getMaterial(Mth::floor(x), fy + 2, Mth::floor(z))->blocksMotion() && up != Tile::unbreakable->id) {
				Step b; b.kind = Step::BREAK; b.x = Mth::floor(x); b.y = fy + 2; b.z = Mth::floor(z); b.tile = -1; b.data = 0; b.ticks = 0;
				digQueue.push_back(b);
			}
			route.destroy();
		}
		progDist = dist;
	}
	int itx = Mth::floor(tx), ity = Mth::floor(ty), itz = Mth::floor(tz);
	// far away: path to a point on the way (the path finder only looks so far)
	if (d2 > 36 * 36) {
		const float d = Mth::sqrt(d2);
		itx = Mth::floor(x + dx / d * 30); itz = Mth::floor(z + dz / d * 30);
		ity = groundY(level, itx, itz);
	}
	// stuck?
	if ((x - lastX) * (x - lastX) + (z - lastZ) * (z - lastZ) < 0.0025f) {
		if (++stuckTicks > 30) { jumping = true; if (stuckTicks > 60) { route.destroy(); routeFails++; stuckTicks = 0; directTicks = 0; } }
	} else stuckTicks = 0;
	lastX = x; lastZ = z;

	// the last couple of blocks: just walk there
	if (d2 < 2.5f * 2.5f && std::fabs(dy) < 1.2f) {
		route.destroy();
		yRot = (float)(Mth::atan2(dz, dx) * 180 / Mth::PI) - 90;
		yya = runSpeed * Mth::Min(1.0f, Mth::sqrt(d2) * 0.8f + 0.2f);
		if (horizontalCollision) jumping = true;
		return false;
	}
	if (directTicks > 0) {
		// no path last time: walk straight at it (jumping), it often works out
		directTicks--;
		yRot = (float)(Mth::atan2(dz, dx) * 180 / Mth::PI) - 90;
		xRot *= 0.8f;
		yya = runSpeed;
		if (horizontalCollision || isInWater()) jumping = true;
		return false;
	}
	const bool far = std::abs(itx - routeTx) + std::abs(itz - routeTz) > (d2 > 36 * 36 ? 8 : 0) || ity != routeTy;
	if (route.isEmpty() || far || ++routeAge > 100) {
		routeTx = itx; routeTy = ity; routeTz = itz; routeAge = 0;
		if (!level->findPath(&route, this, itx, ity, itz, 40.0f, true, false) || route.isEmpty()) {
			routeFails++;
			route.destroy();
			directTicks = 40;
			return false;
		}
	}
	followRoute();
	return false;
}

void PlayerNPC::followRoute()
{
	if (route.isEmpty()) return;
	Vec3 target = route.currentPos(this);
	const float r = bbWidth * 2;
	while (target.distanceToSqr(x, target.y, z) < r * r) {
		route.next();
		if (route.isDone()) { route.destroy(); return; }
		target = route.currentPos(this);
	}
	const float xd = target.x - x, zd = target.z - z;
	const float yd = target.y - Mth::floor(bb.y0 + 0.5f);
	float yRotD = (float)(Mth::atan2(zd, xd) * 180 / Mth::PI) - 90;
	float diff = yRotD - yRot;
	while (diff < -180) diff += 360;
	while (diff >= 180) diff -= 360;
	if (diff > 30) diff = 30;
	if (diff < -30) diff = -30;
	yRot += diff;
	xRot *= 0.8f;
	yya = runSpeed;
	if (yd > 0 || horizontalCollision) jumping = true;
}

void PlayerNPC::lookAtBlock(int bx, int by, int bz)
{
	const float dx = bx + 0.5f - x, dy = by + 0.5f - y, dz = bz + 0.5f - z;
	const float h = Mth::sqrt(dx * dx + dz * dz);
	yRot = (float)(Mth::atan2(dz, dx) * 180 / Mth::PI) - 90;
	xRot = -(float)(Mth::atan2(dy, h) * 180 / Mth::PI);
	yBodyRot = yRot;
}

int PlayerNPC::digTicks(int t)
{
	Tile* tile = Tile::tiles[t];
	if (!tile || tile->destroySpeed < 0) return -1;
	float sec = tile->destroySpeed * 1.5f;
	const Material* m = tile->material;
	if (m == Material::stone) {
		if (pick == 0) sec *= 3.3f;
		else sec /= (pick == 2 ? 4.0f : 2.0f);
	} else if (m == Material::wood) {
		if (axe) sec /= (axe == 2 ? 4.0f : 2.0f);
	}
	return Mth::Max(3, (int)(sec * 20));
}

int PlayerNPC::itemFor(int t) const
{
	if (t == Tile::wood->id) return I_PLANK;
	if (t == Tile::stoneBrick->id) return I_COBBLE;
	if (Tile::isLog(t)) return I_LOG;
	if (t == Tile::dirt->id) return I_DIRT;
	if (t == Tile::fence->id) return I_FENCE;
	if (t == Tile::fenceGate->id) return I_GATE;
	if (t == Tile::door_wood->id) return I_DOOR;
	if (t == Tile::torch->id) return I_TORCH;
	if (t == Tile::crops->id) return I_SEEDS;
	if (t == Tile::workBench->id) return I_TABLE;
	if (t == Tile::furnace->id) return I_FURNACE;
	return -1;
}

void PlayerNPC::collect(int t, int data)
{
	if (Tile::isLog(t)) inv[I_LOG]++;
	else if (t == Tile::rock->id || t == Tile::stoneBrick->id) inv[I_COBBLE]++;
	else if (t == ((Tile*)Tile::grass)->id || t == Tile::dirt->id) inv[I_DIRT]++;
	else if (t == Tile::wood->id) inv[I_PLANK]++;
	else if (t == Tile::coalOre->id) {
		inv[I_COAL]++;
		if (random.nextInt(3) == 0 && lastTopicSaid != T_COAL) say(SAY_LINES[T_COAL][random.nextInt(6)], T_COAL);
	} else if (t == Tile::ironOre->id) {
		inv[I_IRON]++;
		if (random.nextInt(2) == 0 && lastTopicSaid != T_IRON) say(SAY_LINES[T_IRON][random.nextInt(6)], T_IRON);
	} else if (t == Tile::tallgrass->id) { if (random.nextInt(2) == 0) inv[I_SEEDS]++; }
	else if (t == Tile::crops->id) { if (data >= 7) { inv[I_WHEAT]++; inv[I_SEEDS] += 1 + random.nextInt(2); } else inv[I_SEEDS]++; }
	else if (t == Tile::sand->id) inv[I_SAND]++;
	else if (t == Tile::workBench->id) inv[I_TABLE]++;
	else if (t == Tile::fence->id) inv[I_FENCE]++;
}

void PlayerNPC::setHeld(int t, bool block)
{
	if (block) {
		if (t > 0 && t < 256 && Tile::tiles[t]) held = ItemInstance(Tile::tiles[t]);
		else if (t == -100) held = ItemInstance(Item::seeds_wheat);
		else held = ItemInstance();
		if (t == Tile::door_wood->id) held = ItemInstance(Item::door_wood);
		if (t == Tile::crops->id) held = ItemInstance(Item::seeds_wheat);
		return;
	}
	// tool for breaking t
	Tile* tile = (t > 0 && t < 256) ? Tile::tiles[t] : NULL;
	if (tile && tile->material == Material::stone && pick) held = ItemInstance(pick == 2 ? Item::pickAxe_stone : Item::pickAxe_wood);
	else if (tile && tile->material == Material::wood && axe) held = ItemInstance(axe == 2 ? Item::hatchet_stone : Item::hatchet_wood);
	else held = ItemInstance();
}

// would breaking this let lava or water in, or open a drop into a cave?
bool PlayerNPC::mineUnsafe(int bx, int by, int bz)
{
	static const int d[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (int i = 0; i < 6; ++i)
		if (level->getMaterial(bx + d[i][0], by + d[i][1], bz + d[i][2])->isLiquid()) return true;
	// the floor of the next step: something has to be under it
	const int below = level->getTile(bx, by - 1, bz);
	if (by < Mth::floor(y - heightOffset) && below == 0 && level->getTile(bx, by - 2, bz) == 0) return true;
	return false;
}

// get within reach of a block: 1 there, 0 on the way, -1 can't
int PlayerNPC::approach(int bx, int by, int bz)
{
	static const bool log = getenv("MCPE_NPCLOG") != NULL;
	if (inReach(bx, by, bz)) { standValid = false; return 1; }
	if (routeFails > 5) { if (log) printf("[npc] %s gave up walking to %d,%d,%d for %d,%d,%d\n", getNpcName().c_str(), standX, standY, standZ, bx, by, bz); standValid = false; return -1; }
	if (standForX != bx || standForY != by || standForZ != bz) standExcluded.clear();
	if (!standValid || standForX != bx || standForY != by || standForZ != bz || (stuckTicks > 50)) {
		if (!standSpotFor(bx, by, bz, standX, standY, standZ)) { if (log) printf("[npc] %s no stand spot for %d,%d,%d (at %.1f,%.1f,%.1f)\n", getNpcName().c_str(), bx, by, bz, x, y - heightOffset, z); standValid = false; return -1; }
		standValid = true; standForX = bx; standForY = by; standForZ = bz;
	}
	if (walkTo((float)standX, (float)standY, (float)standZ, 0.5f)) {
		// standing there and still too far: try another spot
		routeFails++;
		standValid = false;
		standExcluded.push_back((standX & 0x3ff) | ((standY & 0xff) << 10) | ((standZ & 0x3ff) << 18));
	}
	return 0;
}

void PlayerNPC::popStep()
{
	if (!steps.empty()) steps.pop_front();
	progTimer = 0; progDist = 1e9f;
	routeFails = 0;
	stuckTicks = 0;
	directTicks = 0;
}

void PlayerNPC::runStep()
{
	if (!digQueue.empty()) {
		for (unsigned int i = 0; i < digQueue.size(); ++i) steps.push_front(digQueue[i]);
		digQueue.clear();
		return;
	}
	Step& s = steps.front();
	switch (s.kind) {
	case Step::SAY:
		say(s.text);
		popStep();
		return;

	case Step::WAIT:
		if (s.data == 1 && swingTimer == 0) { swing(); swingTimer = 8; }
		if (s.data == 2 && random.nextInt(30) == 0) { yRot += random.nextFloat() * 120 - 60; xRot = random.nextFloat() * 40 - 20; }
		if (job == JOB_NIGHT && level->isDay()) { steps.clear(); return; }
		if (--s.ticks <= 0) popStep();
		return;

	case Step::CRAFT:
		if (s.data == 1) lookAtBlock(tableX, tableY, tableZ);
		else if (s.ticks % 20 == 0) xRot = 50; // looking at its inventory
		if (s.data == 1 && swingTimer == 0) { swing(); swingTimer = 6; }
		held = ItemInstance();
		if (--s.ticks <= 0) { craftAll(s.data == 1 || !hasTable); popStep(); }
		return;

	case Step::MOVE: {
		const int r = walkTo((float)s.x, (float)s.y, (float)s.z, (float)Mth::Max(1, s.ticks));
		if (r || routeFails > 5) {
			routeFails = 0;
			const Step done = s;
			popStep();
			if (done.data == 1 && !r) { mineFails++; steps.clear(); return; } // couldn't get down there: give up this time
			// in the mine: grab ore in the walls around
			if (r && done.data == 1) {
				mineDepth = Mth::Max(mineDepth, done.tile);
				mineFails = 0;
				const int fx = Mth::floor(x), fy = Mth::floor(y - heightOffset), fz = Mth::floor(z);
				for (int dy = 0; dy <= 1; ++dy) for (int k = 0; k < 4; ++k) {
					const int ox = fx + (k == 0) - (k == 1), oz = fz + (k == 2) - (k == 3);
					const int t = level->getTile(ox, fy + dy, oz);
					if (t == Tile::coalOre->id || t == Tile::ironOre->id) { Step b = done; b.kind = Step::BREAK; b.x = ox; b.y = fy + dy; b.z = oz; b.tile = -1; steps.push_front(b); }
				}
				// lava or water ahead: stop this mine
				const int t = level->getTile(fx, fy - 1, fz);
				if (t == Tile::lava->id || t == Tile::calmLava->id) { steps.clear(); hasMine = false; }
			}
		}
		return;
	}

	case Step::BREAK: {
		const int t = level->getTile(s.x, s.y, s.z);
		if (t == 0 || (s.tile > 0 && t != s.tile) || (s.tile == -1 && level->getMaterial(s.x, s.y, s.z)->isLiquid())) { popStep(); digProgress = 0; return; }
		// wood steps stop at the top of the trunk
		if (job == JOB_WOOD && !Tile::isLog(t)) { popStep(); return; }
		if (t == Tile::unbreakable->id || t == Tile::chest->id || t == Tile::invisible_bedrock->id) { popStep(); return; }
		if (job == JOB_MINE && mineUnsafe(s.x, s.y, s.z)) { steps.clear(); hasMine = false; return; }
		{
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) { markBad(s.x, s.y, s.z); popStep(); if (job == JOB_WOOD || job == JOB_MINE) { if (job == JOB_MINE) mineFails++; steps.clear(); } return; }
			if (a == 0) return;
		}
		route.destroy();
		lookAtBlock(s.x, s.y, s.z);
		setHeld(t, false);
		if (digProgress == 0) digNeeded = digTicks(t);
		if (digNeeded < 0) { popStep(); return; }
		if (swingTimer == 0) { swing(); swingTimer = 5; }
		if ((digProgress & 3) == 0) level->levelEvent(NULL, LevelEvent::NPC_CRACK_BLOCK, s.x, s.y, s.z, 1);
		if (++digProgress >= digNeeded) {
			const int data = level->getData(s.x, s.y, s.z);
			Tile* tile = Tile::tiles[t];
			level->levelEvent(NULL, LevelEvent::PARTICLES_DESTROY_BLOCK, s.x, s.y, s.z, t);
			if (tile) level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, tile->soundType->getBreakSound(), (tile->soundType->getVolume() + 1) / 2, tile->soundType->getPitch() * 0.8f);
			level->setTile(s.x, s.y, s.z, 0);
			collect(t, data);
			digProgress = 0;
			popStep();
		}
		return;
	}

	case Step::TILL: {
		const int t = level->getTile(s.x, s.y, s.z);
		if (t == Tile::farmland->id) { popStep(); return; }
		if (t != Tile::dirt->id && t != ((Tile*)Tile::grass)->id) { popStep(); return; }
		if (!soft(level, s.x, s.y + 1, s.z)) { popStep(); return; }
		if (level->getTile(s.x, s.y + 1, s.z) != 0) { Step b = s; b.kind = Step::BREAK; b.y = s.y + 1; b.tile = -1; steps.push_front(b); return; }
		{
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) { markBad(s.x, s.y, s.z); popStep(); return; }
			if (a == 0) return;
		}
		route.destroy();
		lookAtBlock(s.x, s.y, s.z);
		held = ItemInstance(hoe == 2 ? Item::hoe_stone : Item::hoe_wood);
		if (swingTimer == 0) { swing(); swingTimer = 6; s.ticks++; }
		if (s.ticks >= 2) {
			if (t != Tile::dirt->id && random.nextInt(3) == 0) inv[I_SEEDS]++; // seeds out of the grass
			level->setTile(s.x, s.y, s.z, Tile::farmland->id);
			level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, "step.gravel", 1, 0.8f);
			popStep();
		}
		return;
	}

	case Step::PLACE: {
		const int cur = level->getTile(s.x, s.y, s.z);
		const bool optional = s.data == -2;
		int data = s.data < 0 ? 0 : s.data;
		if (s.tile == Tile::torch->id && data == 0) data = 5;               // standing torch
		if (s.tile == Tile::furnace->id && data == 0) data = 2 + random.nextInt(4);
		if (cur == s.tile && (s.tile != Tile::fenceGate->id || level->getData(s.x, s.y, s.z) == data)) { popStep(); return; }
		if (cur == s.tile && s.tile == Tile::fenceGate->id) {
			// open or close the gate
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) { popStep(); return; }
			if (a == 0) return;
			lookAtBlock(s.x, s.y, s.z);
			swing();
			level->setData(s.x, s.y, s.z, data);
			level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, (data & 4) ? "random.door_open" : "random.door_close", 1, level->random.nextFloat() * 0.1f + 0.9f);
			popStep();
			return;
		}
		// water: the "bucket" comes from the nearest water
		if (s.tile == Tile::calmWater->id && !hasWater) {
			int bx = 0, by = 0, bz = 0; bool found = false;
			const int px = Mth::floor(x), pz = Mth::floor(z);
			for (int i = 0; i < 300 && !found; ++i) {
				const int cx = px + random.nextInt(81) - 40, cz = pz + random.nextInt(81) - 40;
				if (cx < 1 || cz < 1 || cx >= LEVEL_WIDTH - 1 || cz >= LEVEL_DEPTH - 1 || !level->isChunkLoaded(cx >> 4, cz >> 4)) continue;
				const int gy = groundY(level, cx, cz);
				const int wt = level->getTile(cx, gy - 1, cz);
				if (wt == Tile::calmWater->id || wt == Tile::water->id) { bx = cx; by = gy; bz = cz; found = true; }
				else if (level->getTile(cx, gy, cz) == Tile::calmWater->id) { bx = cx; by = gy + 1; bz = cz; found = true; }
			}
			if (!found) { popStep(); waterPlaced = true; return; } // no water around: dry farm
			Step go; go.kind = Step::MOVE; go.x = bx; go.y = by; go.z = bz; go.tile = 0; go.data = 0; go.ticks = 3;
			Step fill; fill.kind = Step::WAIT; fill.x = fill.y = fill.z = 0; fill.tile = 0; fill.data = 1; fill.ticks = 15;
			steps.push_front(fill);
			steps.push_front(go);
			hasWater = true; // after the walk and the swing
			return;
		}
		// filling holes: any block will do
		if (s.data == -1 && itemFor(s.tile) >= 0 && inv[itemFor(s.tile)] <= 0) {
			if (inv[I_DIRT] > 0) s.tile = Tile::dirt->id;
			else if (inv[I_COBBLE] > 0) s.tile = Tile::stoneBrick->id;
			else if (inv[I_PLANK] > 8) s.tile = Tile::wood->id;
			else { popStep(); return; }
		}
		const int item = itemFor(s.tile);
		if (item >= 0 && inv[item] <= 0) {
			if (optional) { popStep(); return; }
			// nothing it goes out to gather: do without
			if (item != I_PLANK && item != I_COBBLE && item != I_LOG && item != I_FENCE && item != I_GATE) { popStep(); return; }
			// out of material: stop here and go get more (the plan remembers where it was)
			if ((job == JOB_BUILD || job == JOB_EXTRA) && s.ticks < (int)planPos) planPos = s.ticks;
			steps.clear();
			job = JOB_NONE;
			return;
		}
		if (s.tile == Tile::crops->id && level->getTile(s.x, s.y - 1, s.z) != Tile::farmland->id) { popStep(); return; }
		// something in the way: break it first
		if (cur != 0 && !soft(level, s.x, s.y, s.z) && !level->getMaterial(s.x, s.y, s.z)->isLiquid()) {
			if (cur == Tile::chest->id || cur == Tile::unbreakable->id) { popStep(); return; }
			Step b = s; b.kind = Step::BREAK; b.tile = -1; steps.push_front(b); return;
		}
		if (cur != 0 && soft(level, s.x, s.y, s.z) && s.tile != Tile::calmWater->id && cur != Tile::calmWater->id && cur != Tile::water->id) {
			Step b = s; b.kind = Step::BREAK; b.tile = -1; steps.push_front(b); return;
		}
		{
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) { markBad(s.x, s.y, s.z); popStep(); return; } // skip this one
			if (a == 0) return;
		}
		// don't place it inside ourselves
		const AABB block((float)s.x, (float)s.y, (float)s.z, s.x + 1.0f, s.y + 1.0f, s.z + 1.0f);
		if (Tile::tiles[s.tile] && Tile::tiles[s.tile]->material->blocksMotion() && bb.intersects(block)) {
			int sx, sy, sz;
			if (standSpotFor(s.x, s.y, s.z, sx, sy, sz)) walkTo((float)sx, (float)sy, (float)sz, 0.4f);
			else popStep();
			return;
		}
		route.destroy();
		lookAtBlock(s.x, s.y, s.z);
		setHeld(s.tile, true);
		if (swingTimer > 0) return; // a moment between blocks, like a real player
		swing();
		swingTimer = 4 + random.nextInt(4);
		if (s.tile == Tile::door_wood->id) {
			level->setTileAndData(s.x, s.y, s.z, s.tile, data);
			level->setTileAndData(s.x, s.y + 1, s.z, s.tile, 8);
		} else {
			level->setTileAndData(s.x, s.y, s.z, s.tile, data);
		}
		Tile* placed = Tile::tiles[s.tile];
		if (placed) level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, placed->soundType->getStepSound(), (placed->soundType->getVolume() + 1) / 2, placed->soundType->getPitch() * 0.8f);
		if (item >= 0) inv[item]--;
		if (s.tile == Tile::workBench->id) { hasTable = true; tableX = s.x; tableY = s.y; tableZ = s.z; }
		if (s.tile == Tile::calmWater->id) { hasWater = false; waterPlaced = true; }
		popStep();
		return;
	}

	case Step::PILLAR: {
		const int fy = Mth::floor(y - heightOffset);
		if (++s.ticks > 60 || std::abs(Mth::floor(x) - s.x) + std::abs(Mth::floor(z) - s.z) > 0) { popStep(); return; }
		if (level->getMaterial(s.x, s.y + 2, s.z)->blocksMotion()) { popStep(); return; } // the head block gets dug first
		jumping = true;
		xRot = 80;
		if (fy >= s.y + 1 && level->getTile(s.x, s.y, s.z) == 0) {
			const int item = inv[I_DIRT] > 0 ? I_DIRT : inv[I_COBBLE] > 0 ? I_COBBLE : I_PLANK;
			const int t = item == I_DIRT ? Tile::dirt->id : item == I_COBBLE ? Tile::stoneBrick->id : Tile::wood->id;
			if (inv[item] <= 0) { popStep(); return; }
			setHeld(t, true);
			swing();
			level->setTile(s.x, s.y, s.z, t);
			Tile* placed = Tile::tiles[t];
			level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, placed->soundType->getStepSound(), (placed->soundType->getVolume() + 1) / 2, placed->soundType->getPitch() * 0.8f);
			inv[item]--;
			popStep();
		}
		return;
	}

	case Step::LEAD: {
		Entity* e = level->getEntity(s.data);
		if (!e || !e->isAlive() || --s.ticks <= 0) { popStep(); return; }
		if (job == JOB_FIGHT) {
			// hit it until it dies or runs
			if (e->distanceToSqr(this) > 24 * 24) { popStep(); fightTarget = 0; return; }
			held = ItemInstance(sword == 2 ? Item::sword_stone : sword == 1 ? Item::sword_wood : NULL);
			if (sword == 0) held = ItemInstance();
			if (e->distanceToSqr(this) > 2.2f * 2.2f) { walkTo(e->x - 0.5f, e->y - e->heightOffset, e->z - 0.5f, 1.5f); return; }
			lookAt(e, 30, 30);
			if (swingTimer == 0) { swing(); swingTimer = 10; e->hurt(this, sword == 2 ? 5 : sword == 1 ? 4 : 1); }
			return;
		}
		// lead an animal into the pen: it follows us, we walk inside
		const bool inside = e->x >= penX && e->x < penX + penW && e->z >= penZ && e->z < penZ + penD;
		if (inside) { popStep(); return; }
		held = ItemInstance(Item::wheat);
		const float d2 = e->distanceToSqr(this);
		if (d2 > 3.5f * 3.5f) { walkTo(e->x - 0.5f, e->y - e->heightOffset, e->z - 0.5f, 2.0f); return; }
		walkTo((float)(penX + penW / 2), (float)penY, (float)(penZ + penD / 2), 1.0f);
		if ((s.ticks % 10) == 0 && e->isMob()) {
			Path p;
			if (level->findPath(&p, e, this, 16, false, true)) ((PathfinderMob*)e)->setPath(p);
		}
		return;
	}
	}
}

// helpers working on someone else's build pull blocks from their plan
void PlayerNPC::windowShifted(int dx, int dz)
{
	homeX += dx; homeZ += dz;
	tableX += dx; tableZ += dz;
	planX += dx; planZ += dz;
	farmX += dx; farmZ += dz;
	penX += dx; penZ += dz;
	mineX += dx; mineZ += dz;
	for (unsigned int i = 0; i < plan.size(); ++i) { plan[i].x += dx; plan[i].z += dz; }
	for (unsigned int i = 0; i < steps.size(); ++i) { steps[i].x += dx; steps[i].z += dz; }
	route.destroy();
}

// ---------------------------------------------------------------------------
// chat
// ---------------------------------------------------------------------------
void PlayerNPC::say(const std::string& text, int topic)
{
	if (text.empty()) return;
	const int now = (int)level->getTime();
	if (now - s_lastChatTime < 30 && topic >= 0) {
		// someone just talked: say it a little later
		pendingLine = text; pendingTopic = topic; pendingDelay = 40 + random.nextInt(60);
		return;
	}
	s_lastChatTime = now;
	level->chatMessage("<" + getNpcName() + "> " + text);
	chatCooldown = 20 * 30 + random.nextInt(20 * 60);
	if (topic >= 0) {
		lastTopicSaid = topic;
		// maybe someone answers
		std::vector<PlayerNPC*> o = others();
		if (!o.empty() && random.nextInt(3) != 0) o[random.nextInt((int)o.size())]->reply(topic, this);
	}
}

void PlayerNPC::reply(int topic, PlayerNPC* to)
{
	if (topic < 0 || topic >= T_COUNT) return;
	std::string line = REPLY_LINES[topic][random.nextInt(4)];
	if (line.empty()) return;
	if (topic == T_HELLO && to && random.nextInt(2) == 0) line += " " + to->getNpcName();
	if (topic == T_DEATH && to) line = (random.nextInt(2) ? "rip " : "noo ") + to->getNpcName();
	if (!pendingLine.empty()) return;
	pendingLine = line;
	pendingTopic = -1;
	pendingDelay = 30 + random.nextInt(80);
}

void PlayerNPC::chatTick()
{
	if (chatCooldown > 0) chatCooldown--;
	if (!pendingLine.empty() && --pendingDelay <= 0) {
		const std::string line = pendingLine;
		const int topic = pendingTopic;
		pendingLine.clear();
		s_lastChatTime = -100000; // let it through
		say(line, topic);
	}
	// now and then, just talk
	if (chatCooldown == 0 && random.nextInt(20 * 90) == 0) say(SAY_LINES[T_BANTER][random.nextInt(6)], T_BANTER);
	if (!level->isDay() && job != JOB_NIGHT && houseDone && random.nextInt(20 * 20) == 0) { steps.clear(); job = JOB_NONE; }
}

// ---------------------------------------------------------------------------
// saving: places are stored relative to the NPC (infinite worlds move)
// ---------------------------------------------------------------------------
void PlayerNPC::addAdditonalSaveData(CompoundTag* tag)
{
	super::addAdditonalSaveData(tag);
	const int bx = Mth::floor(x), by = Mth::floor(y), bz = Mth::floor(z);
	tag->putInt("Number", number);
	for (int i = 0; i < I_COUNT; ++i) { char k[16]; sprintf(k, "Inv%d", i); tag->putInt(k, inv[i]); }
	tag->putInt("Axe", axe); tag->putInt("Pick", pick); tag->putInt("Hoe", hoe); tag->putInt("Sword", sword);
	tag->putByte("HasHome", hasHome); tag->putInt("HomeX", homeX - bx); tag->putInt("HomeY", homeY - by); tag->putInt("HomeZ", homeZ - bz);
	tag->putByte("HasTable", hasTable); tag->putInt("TableX", tableX - bx); tag->putInt("TableY", tableY - by); tag->putInt("TableZ", tableZ - bz);
	tag->putByte("HouseDone", houseDone); tag->putByte("FarmDone", farmDone); tag->putByte("PenDone", penDone); tag->putByte("Water", waterPlaced);
	tag->putInt("Extras", extrasBuilt);
	const bool building = !plan.empty() && job == JOB_BUILD;
	tag->putByte("Building", building);
	tag->putInt("PlanSeed", planSeed); tag->putInt("PlanPos", (int)planPos);
	tag->putInt("PlanX", planX - bx); tag->putInt("PlanY", planY - by); tag->putInt("PlanZ", planZ - bz); tag->putInt("PlanW", planW); tag->putInt("PlanD", planD);
	tag->putInt("FarmX", farmX - bx); tag->putInt("FarmY", farmY - by); tag->putInt("FarmZ", farmZ - bz); tag->putInt("FarmW", farmW); tag->putInt("FarmD", farmD);
	tag->putInt("PenX", penX - bx); tag->putInt("PenY", penY - by); tag->putInt("PenZ", penZ - bz); tag->putInt("PenW", penW); tag->putInt("PenD", penD);
	tag->putInt("MineX", mineX - bx); tag->putInt("MineY", mineY - by); tag->putInt("MineZ", mineZ - bz); tag->putInt("MineDir", mineDir); tag->putByte("HasMine", hasMine); tag->putInt("MineDepth", mineDepth);
}

void PlayerNPC::readAdditionalSaveData(CompoundTag* tag)
{
	super::readAdditionalSaveData(tag);
	const int bx = Mth::floor(x), by = Mth::floor(y), bz = Mth::floor(z);
	setNumber(tag->getInt("Number"));
	for (int i = 0; i < I_COUNT; ++i) { char k[16]; sprintf(k, "Inv%d", i); inv[i] = tag->getInt(k); }
	axe = tag->getInt("Axe"); pick = tag->getInt("Pick"); hoe = tag->getInt("Hoe"); sword = tag->getInt("Sword");
	hasHome = tag->getByte("HasHome") != 0; homeX = tag->getInt("HomeX") + bx; homeY = tag->getInt("HomeY") + by; homeZ = tag->getInt("HomeZ") + bz;
	hasTable = tag->getByte("HasTable") != 0; tableX = tag->getInt("TableX") + bx; tableY = tag->getInt("TableY") + by; tableZ = tag->getInt("TableZ") + bz;
	houseDone = tag->getByte("HouseDone") != 0; farmDone = tag->getByte("FarmDone") != 0; penDone = tag->getByte("PenDone") != 0; waterPlaced = tag->getByte("Water") != 0;
	extrasBuilt = tag->getInt("Extras");
	planSeed = tag->getInt("PlanSeed");
	planX = tag->getInt("PlanX") + bx; planY = tag->getInt("PlanY") + by; planZ = tag->getInt("PlanZ") + bz; planW = tag->getInt("PlanW"); planD = tag->getInt("PlanD");
	farmX = tag->getInt("FarmX") + bx; farmY = tag->getInt("FarmY") + by; farmZ = tag->getInt("FarmZ") + bz; farmW = tag->getInt("FarmW"); farmD = tag->getInt("FarmD");
	penX = tag->getInt("PenX") + bx; penY = tag->getInt("PenY") + by; penZ = tag->getInt("PenZ") + bz; penW = tag->getInt("PenW"); penD = tag->getInt("PenD");
	mineX = tag->getInt("MineX") + bx; mineY = tag->getInt("MineY") + by; mineZ = tag->getInt("MineZ") + bz; mineDir = tag->getInt("MineDir"); hasMine = tag->getByte("HasMine") != 0; mineFails = 0; mineDepth = tag->getInt("MineDepth");
	if (!houseDone && planW > 0) {
		// the same house as before, picking up where it was
		makeHouseDesign(planSeed);
		planPos = 0; // finished blocks are skipped automatically
	}
}
