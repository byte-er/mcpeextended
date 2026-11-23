// Player NPCs: deciding what to do next, and turning it into steps.
#include "PlayerNPC.h"
#include "NpcChat.h"
#include "../EntityTypes.h"
#include "../../level/Level.h"
#include "../../level/tile/Tile.h"
#include "../../level/material/Material.h"
#include "../../level/tile/entity/ChestTileEntity.h"
#include "../../item/Item.h"
#include "../../../util/Mth.h"
#include "../../../util/Random.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

using namespace NpcChat;

static int groundY(Level* level, int x, int z)
{
	int y = level->getHeightmap(x, z);
	if (y >= Level::DEPTH) y = Level::DEPTH - 1;
	while (y > 1 && !level->getMaterial(x, y - 1, z)->blocksMotion()) y--;
	return y; // first non-solid block above the ground
}

void PlayerNPC::startJob(int j)
{
	job = j;
	jobTicks = 0;
	static const bool log = getenv("MCPE_NPCLOG") != NULL; // development
	if (log && (j != lastLoggedJob || level->getTime() - lastLogTime > 200)) {
		lastLoggedJob = j; lastLogTime = (int)level->getTime();
		static const char* names[JOB_COUNT] = { "none", "wood", "mine", "seeds", "craft", "table", "build", "help", "farm", "harvest", "pen", "extra", "night", "wander", "fight", "water",
			"cave", "grief", "store", "project", "smelt", "flee" };
		static const char* roles[ROLE_COUNT] = { "builder", "miner", "lumberjack", "farmer", "explorer", "griefer" };
		printf("[npc] %s(%s) t=%ld job=%s at %d,%d,%d log=%d plank=%d cob=%d dirt=%d coal=%d iron=%d ingot=%d dia=%d torch=%d table=%d tools a%d p%d s%d house=%d(%u/%u) farm=%d pen=%d proj=%d\n",
			getNpcName().c_str(), roles[role], (long)level->getTime(), names[j], Mth::floor(x), Mth::floor(y), Mth::floor(z),
			inv[I_LOG], inv[I_PLANK], inv[I_COBBLE], inv[I_DIRT], inv[I_COAL], inv[I_IRON], inv[I_INGOT], inv[I_DIAMOND], inv[I_TORCH], inv[I_TABLE],
			axe, pick, sword, houseDone, planPos, (unsigned)plan.size(), farmDone, penDone, projectId);
		fflush(stdout);
	}
}

// ---------------------------------------------------------------------------
// planner: what to do next
// ---------------------------------------------------------------------------
void PlayerNPC::think()
{
	digQueue.clear();
	NpcTown* town = NpcTown::get(level);
	// the previous job ran out of steps
	if ((job == JOB_BUILD || job == JOB_EXTRA) && !plan.empty() && planPos >= plan.size()) {
		// keep going: planBuild takes a second pass, then finishes
		if (planBuild(job == JOB_EXTRA)) return;
	}
	if (job == JOB_CAVE && planCaveStep()) return;
	if (job == JOB_PROJECT && planProject()) return;
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
	// someone who keeps griefing and is close by: go get them
	if (role != ROLE_GRIEFER && sword > 0 && !grudges.empty()) {
		for (unsigned int i = 0; i < grudges.size(); ++i) {
			if (grudges[i].amount < 5) continue;
			PlayerNPC* g = town->byNumber(grudges[i].number);
			if (g && g->isAlive() && g->distanceToSqr(this) < 16 * 16) {
				fightTarget = g->entityId;
				if (random.nextInt(2) == 0) say(g->getNpcName() + " " + SAY_LINES[T_ACCUSE][random.nextInt(6)], T_ACCUSE);
				startJob(JOB_FIGHT); push(Step::LEAD, 0, 0, 0, 0, fightTarget, 20 * 20);
				return;
			}
		}
	}
	// a town: everyone gets a plot for their house (unless they already built one)
	if (town->active && !hasPlot && !houseDone && planW == 0) {
		int i, j;
		if (town->plotFor(this, i, j)) {
			hasPlot = true; plotI = i; plotJ = j;
			int x0, z0, w, d;
			town->plotRect(i, j, x0, z0, w, d);
			homeX = x0 + w / 2; homeZ = z0 + d / 2; homeY = town->plotLevel(i, j);
			hasHome = true;
			plan.clear();
		}
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
	if (!level->isDay()) {
		// miners and explorers go caving at night (it's dark down there anyway)
		if ((role == ROLE_MINER || role == ROLE_EXPLORER) && pick >= 2 && caveCooldown == 0 && random.nextInt(2) == 0 && planCave()) return;
		if (houseDone && planNight()) return;
	}

	if (planBasics()) return;
	if (town->active && planTownWork()) return;

	// griefers grief now and then (once they've got going)
	if (role == ROLE_GRIEFER && griefCooldown == 0 && random.nextInt(3) == 0 && planGrief()) return;

	// my house (or help a neighbour now and then)
	if (!houseDone) {
		if (plan.empty()) makeHouseDesign(random.nextInt());
		if (random.nextInt(6) == 0 && planHelp()) return;
		int needP = 0, needC = 0, needL = 0, needO = 0;
		missingFor(plan, planPos, needP, needC, needL, needO, 100000);
		needP = Mth::Min(needP, 40); needC = Mth::Min(needC, 32); needL = Mth::Min(needL, 8); // build in parts
		keepLogs = needL;
		if (inv[I_PLANK] < needP && inv[I_LOG] > needL) { if (planCraft()) return; }
		if (inv[I_DOOR] == 0 && inv[I_PLANK] >= 6) { if (planCraft()) return; }
		if (town->active && (inv[I_PLANK] < needP || inv[I_COBBLE] < needC) && storeCooldown == 0 && town->storageReady()
			&& (town->stock(Tile::wood->id) > 16 || town->stock(Tile::stoneBrick->id) > 16 || town->stock(Tile::treeTrunk->id) > 8)) { if (planStore(false)) return; }
		if (inv[I_PLANK] + Mth::Max(0, inv[I_LOG] - needL) * 4 < needP || inv[I_LOG] < needL) { if (planWood()) return; }
		if (inv[I_COBBLE] < needC) { if (planMine()) return; }
		if (planBuild(false)) return;
	}

	// food
	if (farmDone && random.nextInt(3) == 0 && planHarvest()) return;
	if (!farmDone && !(town->active && role != ROLE_FARMER)) {
		// seeds from tall grass if there is some (tilling grass turns some up too)
		if (inv[I_SEEDS] < 6 && seedTries < 3) { seedTries++; if (planSeeds()) return; }
		if (hoe == 0 || inv[I_FENCE] < 28 || inv[I_GATE] < 1) {
			if (inv[I_PLANK] + inv[I_LOG] * 4 >= 8) { if (planCraft()) return; }
			if (planWood()) return;
		} else if (planFarm()) return;
	}
	// animals
	if (!penDone && !(town->active && role != ROLE_FARMER)) {
		if (inv[I_FENCE] < 26 || inv[I_GATE] < 1) {
			if (inv[I_PLANK] + inv[I_LOG] * 4 >= 8) { if (planCraft()) return; }
			if (planWood()) return;
		} else if (planPen()) return;
	}

	// after that: what they like doing, extra builds, helping, collecting, looking around
	const int r = random.nextInt(10);
	if ((role == ROLE_MINER || role == ROLE_EXPLORER) && r < 4 && caveCooldown == 0 && planCave()) return;
	if (role == ROLE_MINER && r < 7 && inv[I_COBBLE] < 128 && planMine()) return;
	if (role == ROLE_LUMBERJACK && r < 6 && inv[I_LOG] < 64 && planWood()) return;
	if (role == ROLE_EXPLORER && r < 6 && planExplore()) return;
	if (r < 3 && extrasBuilt < (role == ROLE_BUILDER ? 6 : 3) && !town->active) {
		if (plan.empty()) makeExtraDesign(random.nextInt());
		if (afford(16, 12, 4) && planBuild(true)) return;
		plan.clear();
	}
	if (r < 5 && planHelp()) return;
	if (r < 6 && inv[I_LOG] < 16 && planWood()) return;
	if (r < 7 && inv[I_COBBLE] < 48 && planMine()) return;
	if (r < 8 && caveCooldown == 0 && pick >= 2 && random.nextInt(3) == 0 && planCave()) return;
	if (r < 8 && inv[I_LOG] > 0 && planCraft()) return;
	planWander();
}

// tools first, like anyone starting out: wood, then stone, then iron (and diamond if lucky)
bool PlayerNPC::planBasics()
{
	const bool woodTools = axe == 0 || pick == 0 || sword == 0;
	const bool stoneTools = pick >= 1 && (pick < 2 || axe < 2 || sword < 2);
	if (woodTools) {
		if (inv[I_PLANK] + inv[I_LOG] * 4 >= 16 && planCraft()) return true;
		return planWood();
	}
	if (stoneTools) {
		if (inv[I_COBBLE] >= 8) { if (inv[I_PLANK] + inv[I_LOG] * 4 + inv[I_STICK] / 2 >= 6) { if (planCraft()) return true; } else if (planWood()) return true; }
		else if (planMine()) return true;
	}
	// iron: smelt it, then iron tools
	if (pick >= 2 && pick < 3) {
		if (inv[I_INGOT] >= 3 && planCraft()) return true;
		if (inv[I_IRON] >= 3 && inv[I_COAL] >= 1 && planSmelt()) return true;
	}
	if (pick == 3 && inv[I_DIAMOND] >= 3 && planCraft()) return true;
	// torches out of coal
	if (inv[I_COAL] > 0 && inv[I_TORCH] < 16 && (inv[I_STICK] > 0 || inv[I_PLANK] > 0) && random.nextInt(3) == 0) {
		startJob(JOB_CRAFT);
		push(Step::CRAFT, 0, 0, 0, 0, 0, 20 + random.nextInt(20));
		return true;
	}
	return false;
}

// a town: put things in storage, help with the big projects, keep the chests stocked
bool PlayerNPC::planTownWork()
{
	NpcTown* town = NpcTown::get(level);
	if (wantsDeposit() && storeCooldown == 0 && town->storageReady() && planStore(true)) return true;
	// somebody has to put the storage chests up first
	if (!town->projects.empty() && town->projects[0].type == NpcProject::STORAGE && !town->projects[0].done
		&& (role == ROLE_BUILDER || houseDone) && random.nextInt(2) == 0) {
		projectId = 0;
		if (planProject()) return true;
	}
	// houses come first (that's the grind part: tools, stuff, a place to live), with a hand on the town now and then
	if (!houseDone && role != ROLE_GRIEFER && (town->phase == 0 || random.nextInt(4) != 0)) return false;
	int chance = 0;
	switch (role) {
	case ROLE_BUILDER:    chance = 8; break;
	case ROLE_FARMER:     chance = 5; break;
	case ROLE_LUMBERJACK: chance = 3; break;
	case ROLE_MINER:      chance = 3; break;
	case ROLE_EXPLORER:   chance = 2; break;
	case ROLE_GRIEFER:    chance = 1; break;
	}
	// before the big projects start everybody grinds: storage first, then stuff for later
	if (town->phase == 0) return false;
	if (random.nextInt(houseDone ? 10 : 40) < chance && planProject()) return true;
	// the chests are running low: the gatherers top them up
	if (role == ROLE_LUMBERJACK && town->stock(Tile::treeTrunk->id) + town->stock(Tile::wood->id) / 4 < 256 && planWood()) return true;
	if (role == ROLE_MINER && town->stock(Tile::stoneBrick->id) < 512 && random.nextInt(3) != 0 && planMine()) return true;
	return false;
}

bool PlayerNPC::wantsDeposit() const
{
	const bool builder = role == ROLE_BUILDER;
	return inv[I_LOG] > (builder ? 48 : 24) || inv[I_COBBLE] > (builder ? 128 : 64) || inv[I_DIRT] > 128
		|| inv[I_PLANK] > 96 || (inv[I_IRON] > 0 && pick >= 3) || inv[I_DIAMOND] > (pick >= 4 ? 0 : 3) || inv[I_BREAD] > 12
		|| inv[I_WHEAT] > 24 || inv[I_GRAVEL] > 32 || inv[I_GOLD] > 0 || inv[I_REDSTONE] > 0 || inv[I_LAPIS] > 0;
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
	const std::vector<PlayerNPC*>& all = NpcTown::get(level)->npcs();
	for (unsigned int i = 0; i < all.size(); ++i) {
		const PlayerNPC* n = all[i];
		if (n->removed) continue;
		if (n->hasHome && n != this && std::abs(n->homeX - px) < margin && std::abs(n->homeZ - pz) < margin) return true;
		if (n->planW > 0 && px >= n->planX - margin && px < n->planX + n->planW + margin && pz >= n->planZ - margin && pz < n->planZ + n->planD + margin) return true;
		if (n->farmW > 0 && px >= n->farmX - margin && px < n->farmX + n->farmW + margin && pz >= n->farmZ - margin && pz < n->farmZ + n->farmD + margin) return true;
		if (n->penW > 0 && px >= n->penX - margin && px < n->penX + n->penW + margin && pz >= n->penZ - margin && pz < n->penZ + n->penD + margin) return true;
	}
	NpcTown* town = NpcTown::get(level);
	if (town->active && town->inProjectArea(px, pz, margin)) return true;
	return false;
}

bool PlayerNPC::treeTaken(int tx, int tz)
{
	const std::vector<PlayerNPC*>& all = NpcTown::get(level)->npcs();
	for (unsigned int i = 0; i < all.size(); ++i) {
		const PlayerNPC* n = all[i];
		if (n == this || n->removed || n->job != JOB_WOOD || n->steps.empty()) continue;
		if (n->steps.back().x == tx && n->steps.back().z == tz) return true;
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
				if (Tile::isLog(level->getTile(cx, base - 1, cz)) || level->getTile(cx, base - 1, cz) == Tile::wood->id) break; // someone's building
				if (reserved(cx, cz, 2) || isBad(cx, base, cz)) break;
				if (treeTaken(cx, cz)) break; // somebody's chopping that one
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

// ---------------------------------------------------------------------------
// crafting: a crafting table goes with it everywhere (put down, used, picked up)
// ---------------------------------------------------------------------------
bool PlayerNPC::needsTableFor() const
{
	if (axe == 0 || pick == 0 || sword == 0) return true;
	if ((axe < 2 || pick < 2 || sword < 2) && inv[I_COBBLE] >= 3) return true;
	if (pick < 3 && inv[I_INGOT] >= 3) return true;
	if (pick == 3 && inv[I_DIAMOND] >= 3) return true;
	if (inv[I_DOOR] == 0 && inv[I_PLANK] >= 6) return true;
	if (houseDone && hoe == 0) return true;
	if (houseDone && (!farmDone || !penDone) && (inv[I_FENCE] < 56 || inv[I_GATE] < 2) && inv[I_PLANK] >= 4) return true;
	if (inv[I_FURNACE] == 0 && inv[I_COBBLE] >= 8 && (inv[I_IRON] > 0 || inv[I_SAND] > 0)) return true;
	if (inv[I_WHEAT] >= 3) return true;
	return false;
}

bool PlayerNPC::tableNearby(int& tx, int& ty, int& tz)
{
	// the house's table, or any table close by (the plaza has some)
	if (hasTable && level->getTile(tableX, tableY, tableZ) == Tile::workBench->id
		&& std::abs(tableX - x) + std::abs(tableZ - z) < 20) { tx = tableX; ty = tableY; tz = tableZ; return true; }
	const int px = Mth::floor(x), py = Mth::floor(y - heightOffset), pz = Mth::floor(z);
	for (int dy = -2; dy <= 2; ++dy) for (int dx = -6; dx <= 6; ++dx) for (int dz = -6; dz <= 6; ++dz)
		if (level->getTile(px + dx, py + dy, pz + dz) == Tile::workBench->id) { tx = px + dx; ty = py + dy; tz = pz + dz; return true; }
	return false;
}

bool PlayerNPC::planCraft()
{
	startJob(JOB_CRAFT);
	if (hasTable && level->getTile(tableX, tableY, tableZ) != Tile::workBench->id) hasTable = false;
	// planks, sticks, torches and the table itself come out of the little 2x2 grid
	if (!needsTableFor()) { push(Step::CRAFT, 0, 0, 0, 0, 0, 20 + random.nextInt(30)); return true; }
	int tx, ty, tz;
	if (tableNearby(tx, ty, tz)) {
		push(Step::MOVE, tx, ty, tz, 0, 0, 3);
		push(Step::CRAFT, tx, ty, tz, 0, 1, 40 + random.nextInt(40));
		return true;
	}
	// no table around: (make one,) put mine down right here, craft, take it with me again
	if (inv[I_TABLE] == 0) {
		if (inv[I_PLANK] + inv[I_LOG] * 4 < 4) return false;
		push(Step::CRAFT, 0, 0, 0, 0, 0, 20);
	}
	const int px = Mth::floor(x), py = Mth::floor(y - heightOffset), pz = Mth::floor(z);
	bool found = false;
	for (int r = 1; r <= 3 && !found; ++r)
		for (int dx = -r; dx <= r && !found; ++dx) for (int dz = -r; dz <= r && !found; ++dz) {
			if (std::abs(dx) != r && std::abs(dz) != r) continue;
			for (int dy = -1; dy <= 1 && !found; ++dy) {
				const int cx = px + dx, cy = py + dy, cz = pz + dz;
				if (level->getTile(cx, cy, cz) != 0 || !level->getMaterial(cx, cy - 1, cz)->blocksMotion()) continue;
				if (level->getMaterial(cx, cy - 1, cz)->isLiquid() || isBad(cx, cy, cz)) continue;
				tx = cx; ty = cy; tz = cz; found = true;
			}
		}
	if (!found) { push(Step::CRAFT, 0, 0, 0, 0, 0, 20); return true; }
	push(Step::PLACE, tx, ty, tz, Tile::workBench->id, 0);
	push(Step::CRAFT, tx, ty, tz, 0, 1, 40 + random.nextInt(40));
	push(Step::BREAK, tx, ty, tz, Tile::workBench->id); // and back in the inventory it goes
	if (random.nextInt(4) == 0) say(SAY_LINES[T_CRAFT][random.nextInt(6)], T_CRAFT);
	return true;
}

void PlayerNPC::craftAll(bool atTable)
{
	// what a player would make right now, given what it wants next
	const int wantPlanks = 64;
	if (inv[I_LOG] > 0 && inv[I_PLANK] < wantPlanks) {
		const int keep = houseDone ? 0 : keepLogs; // logs for the house's log blocks
		const int use = Mth::Max(0, Mth::Min(inv[I_LOG] - keep, (wantPlanks - inv[I_PLANK] + 3) / 4));
		inv[I_LOG] -= use; inv[I_PLANK] += use * 4;
		if (inv[I_PLANK] < 4 && inv[I_LOG] > 0) { inv[I_LOG]--; inv[I_PLANK] += 4; }
	}
	// everyone carries a crafting table (not while using one: then it's standing right there)
	if (!atTable && inv[I_TABLE] == 0 && inv[I_PLANK] >= 4) { inv[I_PLANK] -= 4; inv[I_TABLE]++; }
	while (inv[I_STICK] < 8 && inv[I_PLANK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
	while (inv[I_COAL] > 0 && inv[I_STICK] > 0 && inv[I_TORCH] < 32) { inv[I_COAL]--; inv[I_STICK]--; inv[I_TORCH] += 4; }
	if (!atTable) return;
	if (axe == 0 && inv[I_PLANK] >= 3 && inv[I_STICK] >= 2) { inv[I_PLANK] -= 3; inv[I_STICK] -= 2; axe = 1; }
	if (pick == 0 && inv[I_PLANK] >= 3 && inv[I_STICK] >= 2) { inv[I_PLANK] -= 3; inv[I_STICK] -= 2; pick = 1; }
	if (sword == 0 && inv[I_PLANK] >= 2 && inv[I_STICK] >= 1) { inv[I_PLANK] -= 2; inv[I_STICK] -= 1; sword = 1; }
	while (inv[I_STICK] < 6 && inv[I_PLANK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
	bool upgraded = false;
	if (pick < 2 && inv[I_COBBLE] >= 3 && inv[I_STICK] >= 2) { inv[I_COBBLE] -= 3; inv[I_STICK] -= 2; pick = 2; upgraded = true; }
	if (axe < 2 && inv[I_COBBLE] >= 3 && inv[I_STICK] >= 2) { inv[I_COBBLE] -= 3; inv[I_STICK] -= 2; axe = 2; upgraded = true; }
	if (sword < 2 && inv[I_COBBLE] >= 2 && inv[I_STICK] >= 1) { inv[I_COBBLE] -= 2; inv[I_STICK] -= 1; sword = 2; }
	if (pick < 3 && inv[I_INGOT] >= 3 && inv[I_STICK] >= 2) { inv[I_INGOT] -= 3; inv[I_STICK] -= 2; pick = 3; upgraded = true; }
	if (sword < 3 && pick >= 3 && inv[I_INGOT] >= 2 && inv[I_STICK] >= 1) { inv[I_INGOT] -= 2; inv[I_STICK] -= 1; sword = 3; }
	if (axe < 3 && pick >= 3 && inv[I_INGOT] >= 3 && inv[I_STICK] >= 2) { inv[I_INGOT] -= 3; inv[I_STICK] -= 2; axe = 3; }
	if (pick == 3 && inv[I_DIAMOND] >= 3 && inv[I_STICK] >= 2) { inv[I_DIAMOND] -= 3; inv[I_STICK] -= 2; pick = 4; upgraded = true; if (random.nextInt(2) == 0) say("got a diamond pickaxe", T_TOOLS); }
	if (upgraded && random.nextInt(2) == 0) say(SAY_LINES[T_TOOLS][random.nextInt(6)], T_TOOLS);
	if (hoe == 0 && houseDone && inv[I_PLANK] >= 2 && inv[I_STICK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] -= 2; hoe = 1; }
	if (inv[I_DOOR] == 0 && inv[I_PLANK] >= 6) { inv[I_PLANK] -= 6; inv[I_DOOR] += 3; }
	if (houseDone && (!farmDone || !penDone || NpcTown::get(level)->active)) {
		while (inv[I_GATE] < 2 && inv[I_PLANK] >= 2) {
			while (inv[I_STICK] < 4 && inv[I_PLANK] >= 4) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
			if (inv[I_STICK] < 4) break;
			inv[I_PLANK] -= 2; inv[I_STICK] -= 4; inv[I_GATE]++;
		}
		while (inv[I_FENCE] < 56 && inv[I_PLANK] >= 4 && inv[I_STICK] >= 2) {
			inv[I_PLANK] -= 4; inv[I_STICK] -= 2; inv[I_FENCE] += 3;
			while (inv[I_STICK] < 2 && inv[I_PLANK] >= 2) { inv[I_PLANK] -= 2; inv[I_STICK] += 4; }
		}
	}
	if (inv[I_FURNACE] == 0 && inv[I_COBBLE] >= 8 && (inv[I_IRON] > 0 || inv[I_SAND] > 0 || inv[I_COBBLE] >= 24)) { inv[I_COBBLE] -= 8; inv[I_FURNACE]++; }
	while (inv[I_WHEAT] >= 3) { inv[I_WHEAT] -= 3; inv[I_BREAD]++; }
}

// furnace: iron, sand into glass, and (for builders) cobble into stone bricks
void PlayerNPC::smeltAll()
{
	int fuel = inv[I_COAL] * 8 + inv[I_PLANK] / 2;
	int n = Mth::Min(inv[I_IRON], fuel);
	inv[I_IRON] -= n; inv[I_INGOT] += n; fuel -= n;
	int g = Mth::Min(inv[I_SAND], Mth::Min(fuel, 32));
	inv[I_SAND] -= g; inv[I_GLASS] += g; fuel -= g;
	if (role == ROLE_BUILDER && inv[I_COBBLE] > 64) {
		const int b = Mth::Min(inv[I_COBBLE] - 64, Mth::Min(fuel, 48));
		inv[I_COBBLE] -= b; inv[I_STONEBRICK] += b; fuel -= b; // (stone, then 2x2 into bricks)
	}
	const int used = inv[I_COAL] * 8 + inv[I_PLANK] / 2 - fuel;
	const int coalUsed = Mth::Min(inv[I_COAL], (used + 7) / 8);
	inv[I_COAL] -= coalUsed;
	if (n > 0 && random.nextInt(3) == 0) say(SAY_LINES[T_SMELT][random.nextInt(6)], T_SMELT);
}

bool PlayerNPC::planSmelt()
{
	// a furnace near by (house, plaza), or the one in my bag
	const int px = Mth::floor(x), py = Mth::floor(y - heightOffset), pz = Mth::floor(z);
	for (int dy = -2; dy <= 2; ++dy) for (int dx = -8; dx <= 8; ++dx) for (int dz = -8; dz <= 8; ++dz) {
		const int t = level->getTile(px + dx, py + dy, pz + dz);
		if (t == Tile::furnace->id) {
			startJob(JOB_SMELT);
			push(Step::SMELT, px + dx, py + dy, pz + dz, 0, 0, 20 * 8);
			return true;
		}
	}
	if (inv[I_FURNACE] == 0) {
		if (inv[I_COBBLE] < 8) return planMine();
		return planCraft(); // makes one at the table
	}
	for (int r = 1; r <= 2; ++r)
		for (int dx = -r; dx <= r; ++dx) for (int dz = -r; dz <= r; ++dz) {
			if (std::abs(dx) != r && std::abs(dz) != r) continue;
			const int cx = px + dx, cz = pz + dz;
			if (level->getTile(cx, py, cz) != 0 || !level->getMaterial(cx, py - 1, cz)->blocksMotion()) continue;
			startJob(JOB_SMELT);
			push(Step::PLACE, cx, py, cz, Tile::furnace->id, 0);
			push(Step::SMELT, cx, py, cz, 0, 0, 20 * 8);
			push(Step::BREAK, cx, py, cz, -1); // (a lit furnace is a different tile)
			return true;
		}
	return false;
}

bool PlayerNPC::afford(int planks, int cobble, int logs)
{
	return inv[I_PLANK] >= planks && inv[I_COBBLE] >= cobble && inv[I_LOG] >= logs;
}

// what the rest of a plan still needs (blocks not placed yet)
bool PlayerNPC::missingFor(const std::vector<Planned>& pl, unsigned int from, int& needP, int& needC, int& needL, int& needO, int maxScan)
{
	needP = needC = needL = needO = 0;
	int scanned = 0;
	for (unsigned int i = from; i < pl.size() && scanned < maxScan; ++i) {
		const Planned& p = pl[i];
		if (p.data == -2 || p.tile == 0) continue;
		if (p.data == -1) { if (level->getTile(p.x, p.y, p.z) == 0) { needO++; scanned++; } continue; }
		if (level->getTile(p.x, p.y, p.z) == p.tile) continue;
		scanned++;
		if (p.tile == Tile::wood->id || p.tile == Tile::fence->id) needP++;
		else if (p.tile == Tile::stoneBrick->id || p.tile == Tile::stoneBrickSmooth->id || p.tile == Tile::gravel->id) needC++;
		else if (Tile::isLog(p.tile)) needL++;
	}
	return needP + needC + needL + needO > 0;
}

bool PlayerNPC::planMine()
{
	if (pick == 0) return false;
	if (!hasMine || mineY - mineDepth < 14 || mineFails > 2) {
		hasMine = false;
		// a new staircase near home, away from everyone's buildings
		NpcTown* town = NpcTown::get(level);
		for (int tries = 0; tries < 20; ++tries) {
			float ang = random.nextFloat() * 6.2831853f;
			int d = 8 + random.nextInt(6);
			int ox = homeX, oz = homeZ;
			if (town->active && town->inTown(homeX, homeZ, 0)) {
				// a town: start just outside it, on my side of town
				ang = (float)Mth::atan2(homeZ - town->cz(), homeX - town->cx()) + (random.nextFloat() - 0.5f) * 0.8f;
				ox = town->cx(); oz = town->cz();
				d = (int)(town->cells * NpcTown::CELL * 1.45f) + 4 + random.nextInt(8);
			}
			const int mx = ox + (int)(Mth::cos(ang) * d), mz = oz + (int)(Mth::sin(ang) * d);
			const int dir = random.nextInt(4);
			const int ex = mx + (dir == 0 ? 12 : dir == 1 ? -12 : 0), ez = mz + (dir == 2 ? 12 : dir == 3 ? -12 : 0);
			if (mx < 16 || mz < 16 || mx >= LEVEL_WIDTH - 16 || mz >= LEVEL_DEPTH - 16) continue;
			if (ex < 16 || ez < 16 || ex >= LEVEL_WIDTH - 16 || ez >= LEVEL_DEPTH - 16) continue;
			if (reserved(mx, mz, 3) || reserved(ex, ez, 3)) continue;
			if (!level->isChunkLoaded(mx >> 4, mz >> 4) || !level->isChunkLoaded(ex >> 4, ez >> 4)) continue;
			// dry, solid ground: no lake, sea or ice under the stairs
			const int gy = groundY(level, mx, mz);
			bool wet = false;
			for (int k = 0; k <= 12 && !wet; k += 3) {
				const int sx = mx + (ex - mx) * k / 12, sz = mz + (ez - mz) * k / 12;
				const int sy = groundY(level, sx, sz);
				for (int dy = 0; dy <= 4 && !wet; ++dy) {
					const int t = level->getTile(sx, sy - dy, sz);
					if (level->getMaterial(sx, sy - dy, sz)->isLiquid() || t == Tile::ice->id || t == Tile::packedIce->id) wet = true;
				}
				if (std::abs(sy - gy) > 6) wet = true; // a cliff
			}
			if (wet) continue;
			mineX = mx; mineZ = mz; mineDir = dir; mineDepth = 0; mineFails = 0;
			mineY = gy;
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
	push(Step::WAIT, 0, 0, 0, 0, 2, 20 * 30);
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
// caving: find a cave (or dig down to one), wander through it with torches,
// mine the ores in the walls, and find the way back out by the trail
// ---------------------------------------------------------------------------
bool PlayerNPC::planCave()
{
	if (pick == 0) return false;
	if (inv[I_TORCH] < 4 && inv[I_COAL] > 0) { startJob(JOB_CRAFT); push(Step::CRAFT, 0, 0, 0, 0, 0, 20); return true; }
	if (inv[I_TORCH] == 0 && role != ROLE_EXPLORER && role != ROLE_MINER) return false;
	const int px = Mth::floor(x), pz = Mth::floor(z), py = Mth::floor(y - heightOffset);
	int best = 1 << 30, bx = 0, by = 0, bz = 0;
	for (int i = 0; i < 160; ++i) {
		const int cx = px + random.nextInt(57) - 28, cz = pz + random.nextInt(57) - 28;
		if (cx < 4 || cz < 4 || cx >= LEVEL_WIDTH - 4 || cz >= LEVEL_DEPTH - 4 || !level->isChunkLoaded(cx >> 4, cz >> 4)) continue;
		const int top = groundY(level, cx, cz);
		for (int cy = top - 4; cy > Mth::Max(6, top - 40); --cy) {
			if (!standable(cx, cy, cz) || level->getMaterial(cx, cy, cz)->isLiquid()) continue;
			// a real cave, not a little hole: open space around
			int air = 0;
			for (int dx = -2; dx <= 2; ++dx) for (int dz = -2; dz <= 2; ++dz) if (level->getTile(cx + dx, cy, cz + dz) == 0) air++;
			if (air < 10) continue;
			if (isBad(cx, cy, cz) || NpcTown::get(level)->inTown(cx, cz, 2)) break;
			const int d = std::abs(cx - px) + std::abs(cz - pz) + std::abs(cy - py) / 2;
			if (d < best) { best = d; bx = cx; by = cy; bz = cz; }
			break;
		}
	}
	if (best == (1 << 30)) return false;
	startJob(JOB_CAVE);
	caveTrail.clear();
	Waypoint w = { px, py, pz };
	caveTrail.push_back(w);
	caveMoves = 0;
	returning = false;
	if (random.nextInt(2) == 0) say(SAY_LINES[T_CAVE][random.nextInt(6)], T_CAVE);
	push(Step::MOVE, bx, by, bz, 0, 3, 2); // data 3: cave walk (digs its way if it has to)
	return true;
}

bool PlayerNPC::planCaveStep()
{
	if (returning) {
		// back the way we came
		while (!caveTrail.empty()) {
			Waypoint w = caveTrail.back();
			caveTrail.pop_back();
			if (std::abs(w.x - x) + std::abs(w.z - z) + std::abs(w.y - (y - heightOffset)) < 3) continue;
			startJob(JOB_CAVE);
			push(Step::MOVE, w.x, w.y, w.z, 0, 0, 2);
			return true;
		}
		returning = false;
		caveCooldown = 20 * 60 * 2 + random.nextInt(20 * 60 * 3);
		return false;
	}
	const int fx = Mth::floor(x), fy = Mth::floor(y - heightOffset), fz = Mth::floor(z);
	const int maxMoves = role == ROLE_EXPLORER ? 30 : role == ROLE_MINER ? 22 : 12;
	const bool dark = level->getRawBrightness(fx, fy, fz) < 7;
	if (++caveMoves > maxMoves || health < 10 || (inv[I_TORCH] == 0 && dark && caveMoves > 4) || inv[I_COBBLE] > 200) {
		returning = true;
		if (random.nextInt(3) == 0) say(random.nextInt(2) ? "heading back up" : "ok leaving the cave", T_CAVE);
		return planCaveStep();
	}
	startJob(JOB_CAVE);
	// ore in the walls around (that the air touches)
	int ores = 0;
	for (int dy = -4; dy <= 5 && ores < 4; ++dy) for (int dx = -6; dx <= 6 && ores < 4; ++dx) for (int dz = -6; dz <= 6 && ores < 4; ++dz) {
		const int ox = fx + dx, oy = fy + dy, oz = fz + dz;
		const int t = level->getTile(ox, oy, oz);
		if (t != Tile::coalOre->id && t != Tile::ironOre->id && t != Tile::goldOre->id && t != Tile::emeraldOre->id
			&& t != Tile::redStoneOre->id && t != Tile::lapisOre->id) continue;
		if (!canHarvest(t) && t != Tile::coalOre->id) continue;
		if (level->getTile(ox + 1, oy, oz) && level->getTile(ox - 1, oy, oz) && level->getTile(ox, oy + 1, oz) && level->getTile(ox, oy - 1, oz)
			&& level->getTile(ox, oy, oz + 1) && level->getTile(ox, oy, oz - 1)) continue; // buried
		if (isBad(ox, oy, oz)) continue;
		push(Step::BREAK, ox, oy, oz, t);
		ores++;
	}
	// light it up
	if (dark && inv[I_TORCH] > 0 && (std::abs(caveTorchX - fx) + std::abs(caveTorchY - fy) + std::abs(caveTorchZ - fz)) > 7
		&& level->getTile(fx, fy, fz) == 0 && level->getMaterial(fx, fy - 1, fz)->blocksMotion())
		pushFront(Step::PLACE, fx, fy, fz, Tile::torch->id, -2);
	// on, deeper in: a dark spot not too far, away from where we've been
	int best = -1000, bx = 0, by = 0, bz = 0;
	for (int i = 0; i < 90; ++i) {
		const int cx = fx + random.nextInt(21) - 10, cy = fy + random.nextInt(11) - 6, cz = fz + random.nextInt(21) - 10;
		if (!standable(cx, cy, cz) || level->getMaterial(cx, cy, cz)->isLiquid() || isBad(cx, cy, cz)) continue;
		if (cy > level->getHeightmap(cx, cz) - 3) continue; // under ground
		bool visited = false;
		for (unsigned int k = 0; k < caveTrail.size() && !visited; ++k)
			if (std::abs(caveTrail[k].x - cx) + std::abs(caveTrail[k].y - cy) + std::abs(caveTrail[k].z - cz) < 5) visited = true;
		if (visited) continue;
		const int light = level->getRawBrightness(cx, cy, cz);
		const int dist = std::abs(cx - fx) + std::abs(cz - fz);
		const int score = (15 - light) * 2 + (fy - cy) + (dist >= 4 && dist <= 12 ? 6 : 0) - (dist > 14 ? 10 : 0);
		if (score > best) { best = score; bx = cx; by = cy; bz = cz; }
	}
	if (best == -1000 && ores == 0) { returning = true; return planCaveStep(); }
	if (best != -1000) {
		if (caveTrail.empty() || std::abs(caveTrail.back().x - fx) + std::abs(caveTrail.back().y - fy) + std::abs(caveTrail.back().z - fz) > 3) {
			Waypoint w = { fx, fy, fz };
			if (caveTrail.size() < 80) caveTrail.push_back(w);
		}
		push(Step::MOVE, bx, by, bz, 0, 3, 1);
	}
	return true;
}

// ---------------------------------------------------------------------------
// griefers: holes in houses, dirt towers, stealing from the chests, trampled farms
// (only the NPCs' own stuff: they don't know the real player is there)
// ---------------------------------------------------------------------------
bool PlayerNPC::planGrief()
{
	NpcTown* town = NpcTown::get(level);
	std::vector<PlayerNPC*> o = others();
	std::vector<PlayerNPC*> victims;
	for (unsigned int i = 0; i < o.size(); ++i)
		if (o[i]->role != ROLE_GRIEFER && o[i]->planW > 0 && o[i]->houseDone && o[i]->distanceToSqr(this) < 80 * 80) victims.push_back(o[i]);
	const int r = random.nextInt(10);
	PlayerNPC* v = victims.empty() ? NULL : victims[random.nextInt((int)victims.size())];
	griefCooldown = 20 * 60 * (2 + random.nextInt(4));
	if (v && r < 5) {
		// knock a few holes in their house
		startJob(JOB_GRIEF);
		const int side = random.nextInt(4);
		const int n = 3 + random.nextInt(4);
		int gx = 0, gy = 0, gz = 0, added = 0;
		for (int k = 0; k < n * 3 && added < n; ++k) {
			int bx, bz;
			if (side < 2) { bx = v->planX + 1 + random.nextInt(Mth::Max(1, v->planW - 2)); bz = side == 0 ? v->planZ : v->planZ + v->planD - 1; }
			else { bz = v->planZ + 1 + random.nextInt(Mth::Max(1, v->planD - 2)); bx = side == 2 ? v->planX : v->planX + v->planW - 1; }
			const int by = v->planY + random.nextInt(3);
			const int t = level->getTile(bx, by, bz);
			if (t == 0 || t == Tile::chest->id || t == Tile::door_wood->id || !level->getMaterial(bx, by, bz)->blocksMotion()) continue;
			push(Step::BREAK, bx, by, bz, -1);
			if (!added) { gx = bx; gy = by; gz = bz; }
			added++;
		}
		if (!added) return false;
		Step rep; rep.kind = Step::SAY; rep.x = gx; rep.y = gy; rep.z = gz; rep.tile = v->number; rep.data = 7; rep.ticks = 0; rep.proj = -1;
		steps.insert(steps.begin() + 1, rep); // tell everyone once the first block is gone
		lastGriefVictim = v->number;
		return true;
	}
	if (v && r < 7 && inv[I_DIRT] + inv[I_COBBLE] >= 8) {
		// a dirt tower right next to their door
		int tx = v->planX - 2, tz = v->planZ + v->planD / 2;
		if (random.nextInt(2)) tx = v->planX + v->planW + 1;
		const int ty = groundY(level, tx, tz);
		if (!standable(tx, ty, tz)) return false;
		startJob(JOB_GRIEF);
		push(Step::MOVE, tx, ty, tz, 0, 0, 0);
		Step rep; rep.kind = Step::SAY; rep.x = tx; rep.y = ty; rep.z = tz; rep.tile = v->number; rep.data = 7; rep.ticks = 0; rep.proj = -1;
		steps.push_back(rep);
		const int h = 5 + random.nextInt(6);
		for (int k = 0; k < h; ++k) push(Step::PILLAR, tx, ty + k, tz);
		push(Step::WAIT, 0, 0, 0, 0, 2, 40);
		lastGriefVictim = v->number;
		return true;
	}
	if (r < 9 && town->active && town->storageReady()) {
		// "borrow" from the town chests
		int sx, sy, sz;
		town->storagePos(random.nextInt(town->storageCount()), sx, sy, sz);
		startJob(JOB_GRIEF);
		push(Step::CHEST, sx, sy, sz, 0, 2, 30);
		Step rep; rep.kind = Step::SAY; rep.x = sx; rep.y = sy; rep.z = sz; rep.tile = 0; rep.data = 7; rep.ticks = 0; rep.proj = -1;
		steps.push_back(rep);
		return true;
	}
	// trample someone's crops
	for (unsigned int i = 0; i < o.size(); ++i) {
		PlayerNPC* f = o[(i + random.nextInt((int)o.size())) % o.size()];
		if (f->farmW <= 0 || f->role == ROLE_GRIEFER || f->distanceToSqr(this) > 80 * 80) continue;
		startJob(JOB_GRIEF);
		int added = 0;
		for (int k = 0; k < 12 && added < 6; ++k) {
			const int cx = f->farmX + random.nextInt(f->farmW), cz = f->farmZ + random.nextInt(f->farmD);
			if (level->getTile(cx, f->farmY, cz) != Tile::crops->id) continue;
			push(Step::BREAK, cx, f->farmY, cz, Tile::crops->id);
			if (!added) {
				Step rep; rep.kind = Step::SAY; rep.x = cx; rep.y = f->farmY; rep.z = cz; rep.tile = f->number; rep.data = 7; rep.ticks = 0; rep.proj = -1;
				steps.push_back(rep);
			}
			added++;
		}
		if (added) return true;
	}
	return false;
}

bool PlayerNPC::planFlee(Entity* from)
{
	if (!from) return false;
	startJob(JOB_FLEE);
	push(Step::LEAD, 0, 0, 0, 0, from->entityId, 20 * 6);
	return true;
}

// ---------------------------------------------------------------------------
// town storage: real chests on the plaza
// ---------------------------------------------------------------------------
bool PlayerNPC::planStore(bool deposit)
{
	NpcTown* town = NpcTown::get(level);
	if (!town->active || !town->storageReady()) return false;
	// nearest chest
	int best = 1 << 30, bx = 0, by = 0, bz = 0;
	for (int k = 0; k < town->storageCount(); ++k) {
		int sx, sy, sz;
		town->storagePos(k, sx, sy, sz);
		const int d = std::abs(sx - Mth::floor(x)) + std::abs(sz - Mth::floor(z)) + random.nextInt(4);
		if (d < best) { best = d; bx = sx; by = sy; bz = sz; }
	}
	startJob(JOB_STORE);
	push(Step::CHEST, bx, by, bz, 0, deposit ? 0 : 1, 30);
	storeCooldown = 20 * 30 + random.nextInt(20 * 30);
	if (deposit && random.nextInt(5) == 0) say(SAY_LINES[T_STORE][random.nextInt(6)], T_STORE);
	return true;
}

int PlayerNPC::invTile(int k, int& aux) const
{
	aux = 0;
	switch (k) {
	case I_LOG: return Tile::treeTrunk->id;
	case I_PLANK: return Tile::wood->id;
	case I_COBBLE: return Tile::stoneBrick->id;
	case I_DIRT: return Tile::dirt->id;
	case I_SEEDS: return Item::seeds_wheat->id;
	case I_WHEAT: return Item::wheat->id;
	case I_COAL: return Item::coal->id;
	case I_IRON: return Tile::ironOre->id;
	case I_FENCE: return Tile::fence->id;
	case I_TORCH: return Tile::torch->id;
	case I_BREAD: return Item::bread->id;
	case I_SAND: return Tile::sand->id;
	case I_GOLD: return Tile::goldOre->id;
	case I_DIAMOND: return Item::emerald->id;
	case I_LAPIS: aux = 4; return Item::dye_powder->id;
	case I_INGOT: return Item::ironIngot->id;
	case I_GRAVEL: return Tile::gravel->id;
	case I_STONEBRICK: return Tile::stoneBrickSmooth->id;
	case I_GLASS: return Tile::glass->id;
	case I_SAPLING: return Tile::sapling->id;
	default: return 0;
	}
}

void PlayerNPC::useChest(const Step& s)
{
	NpcTown* town = NpcTown::get(level);
	if (s.data == 0) {
		// keep what I need myself, the rest goes in
		int keep[I_COUNT];
		for (int i = 0; i < I_COUNT; ++i) keep[i] = 1 << 20;
		const bool builder = role == ROLE_BUILDER;
		keep[I_LOG] = builder ? 32 : 8; keep[I_PLANK] = builder ? 64 : 24; keep[I_COBBLE] = builder ? 96 : 32; keep[I_DIRT] = 48;
		keep[I_IRON] = pick >= 3 ? 0 : 3; keep[I_DIAMOND] = pick >= 4 ? 0 : 3; keep[I_BREAD] = 6; keep[I_WHEAT] = 2; keep[I_GRAVEL] = builder ? 32 : 0;
		keep[I_GOLD] = 0; keep[I_REDSTONE] = 0; keep[I_LAPIS] = 0; keep[I_SEEDS] = 24; keep[I_COAL] = 16; keep[I_SAND] = 16; keep[I_INGOT] = pick >= 3 && sword >= 3 ? 0 : 5;
		int moved = 0;
		for (int k = 0; k < I_COUNT; ++k) {
			if (inv[k] <= keep[k]) continue;
			int aux;
			const int id = invTile(k, aux);
			if (!id) continue;
			const int put = town->deposit(id, aux, inv[k] - keep[k]);
			inv[k] -= put; moved += put;
		}
		if (moved > 0) swing();
	} else if (s.data == 1) {
		// take what my build needs
		int needP = 0, needC = 0, needL = 0, needO = 0;
		if (job == JOB_PROJECT || projectId >= 0) {
			if (projectId >= 0 && projectId < (int)town->projects.size()) missingFor(town->projects[projectId].plan, town->projects[projectId].pos, needP, needC, needL, needO, 64);
		} else missingFor(plan, planPos, needP, needC, needL, needO, 96);
		needP = Mth::Max(needP, 16) - inv[I_PLANK]; needC = Mth::Max(needC, 16) - inv[I_COBBLE]; needL -= inv[I_LOG]; needO -= inv[I_DIRT] + inv[I_COBBLE];
		if (needP > 0) { int got = town->withdraw(Tile::wood->id, -1, Mth::Min(needP, 128)); inv[I_PLANK] += got; needP -= got;
			if (needP > 0) inv[I_LOG] += town->withdraw(Tile::treeTrunk->id, -1, (needP + 3) / 4); }
		if (needC > 0) inv[I_COBBLE] += town->withdraw(Tile::stoneBrick->id, -1, Mth::Min(needC, 192));
		if (needL > 0) inv[I_LOG] += town->withdraw(Tile::treeTrunk->id, -1, needL);
		if (needO > 0) inv[I_DIRT] += town->withdraw(Tile::dirt->id, -1, Mth::Min(needO, 128));
		if (inv[I_STONEBRICK] < 32) inv[I_STONEBRICK] += town->withdraw(Tile::stoneBrickSmooth->id, -1, 64);
		if (inv[I_GRAVEL] < 16) inv[I_GRAVEL] += town->withdraw(Tile::gravel->id, -1, 32);
		if (inv[I_TORCH] < 8) inv[I_TORCH] += town->withdraw(Tile::torch->id, -1, 16);
		if (inv[I_GLASS] < 8) inv[I_GLASS] += town->withdraw(Tile::glass->id, -1, 16);
		if (inv[I_BREAD] < 2) inv[I_BREAD] += town->withdraw(Item::bread->id, -1, 3);
		if (inv[I_INGOT] < 3 && pick < 3) inv[I_INGOT] += town->withdraw(Item::ironIngot->id, -1, 3);
		swing();
	} else {
		// a griefer helping themself to the good stuff
		inv[I_DIAMOND] += town->withdraw(Item::emerald->id, -1, 8);
		inv[I_INGOT] += town->withdraw(Item::ironIngot->id, -1, 16);
		inv[I_IRON] += town->withdraw(Tile::ironOre->id, -1, 16);
		inv[I_BREAD] += town->withdraw(Item::bread->id, -1, 8);
		inv[I_GOLD] += town->withdraw(Tile::goldOre->id, -1, 16);
		inv[I_TORCH] += town->withdraw(Tile::torch->id, -1, 16);
		swing();
	}
}

// ---------------------------------------------------------------------------
// town projects
// ---------------------------------------------------------------------------
bool PlayerNPC::planProject()
{
	NpcTown* town = NpcTown::get(level);
	if (!town->active) { projectId = -1; return false; }
	NpcProject* p = NULL;
	if (projectId >= 0 && projectId < (int)town->projects.size() && !town->projects[projectId].done) p = &town->projects[projectId];
	else p = town->pickProject(this);
	if (!p) { projectId = -1; return false; }
	const bool fresh = projectId != town->projectIndex(p);
	projectId = town->projectIndex(p);
	if (!p->built) town->buildPlan(*p);
	// materials for the next stretch of it
	int needP = 0, needC = 0, needL = 0, needO = 0;
	missingFor(p->plan, p->pos, needP, needC, needL, needO, 48);
	needP = Mth::Min(needP, 32); needC = Mth::Min(needC, 48);
	if (inv[I_PLANK] < needP && inv[I_LOG] > 0) { if (planCraft()) return true; }
	if ((inv[I_PLANK] < needP || inv[I_COBBLE] + inv[I_STONEBRICK] < needC || inv[I_LOG] < needL) && storeCooldown == 0 && town->storageReady()) {
		if ((needC > inv[I_COBBLE] && town->stock(Tile::stoneBrick->id) > 0) || (needP > inv[I_PLANK] && town->stock(Tile::wood->id) + town->stock(Tile::treeTrunk->id) > 0)
			|| (needL > inv[I_LOG] && town->stock(Tile::treeTrunk->id) > 0)) {
			if (planStore(false)) return true;
		}
	}
	if (inv[I_PLANK] + inv[I_LOG] * 4 < needP || inv[I_LOG] < needL) { if (random.nextInt(2) == 0) say(SAY_LINES[T_NEED][random.nextInt(6)], T_NEED); projectId = -1; return planWood(); }
	if (inv[I_COBBLE] + inv[I_STONEBRICK] < needC && inv[I_COBBLE] < 8) { projectId = -1; return planMine(); }
	startJob(JOB_PROJECT);
	if (fresh && random.nextInt(4) == 0) say(SAY_LINES[T_PROJECT][random.nextInt(6)], T_PROJECT);
	int n = 0;
	NpcPlanned b;
	while (n < 24) {
		const unsigned int idx = p->pos;
		if (!town->claimBlock(p, b)) break;
		Step s; s.x = b.x; s.y = b.y; s.z = b.z; s.tile = b.tile; s.data = b.data; s.ticks = (int)idx; s.proj = projectId;
		if (b.tile == 0) { s.kind = Step::BREAK; s.tile = -1; }
		else if (b.tile == Tile::farmland->id) s.kind = Step::TILL;
		else s.kind = Step::PLACE;
		steps.push_back(s);
		n++;
	}
	if (n == 0) { projectId = -1; return false; }
	return true;
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
	NpcTown* town = NpcTown::get(level);
	const bool onPlot = hasPlot && town->active;
	int w = 5 + r.nextInt(3), d = 5 + r.nextInt(3), h = 3 + r.nextInt(2);
	const bool twoStory = onPlot && r.nextInt(4) == 0;
	if (onPlot) { w += r.nextInt(3); d += r.nextInt(2); }
	if (twoStory) h = 7;
	int px0 = 0, pz0 = 0, pw = 0, pd = 0;
	if (onPlot) {
		town->plotRect(plotI, plotJ, px0, pz0, pw, pd);
		w = Mth::Min(w, pw - 2); d = Mth::Min(d, pd - 2);
	}
	if (planW == 0 || onPlot) {
		// the house goes on the home spot (in the middle of the plot)
		planX = homeX - w / 2; planZ = homeZ - d / 2;
		planY = homeY;
	}
	planW = w; planD = d;
	const int P = Tile::wood->id, C = Tile::stoneBrick->id, L = Tile::treeTrunk->id, S = Tile::stoneBrickSmooth->id;
	const int wallStyle = r.nextInt(onPlot ? 6 : 4);  // planks, cobble, cobble base, logs, (town) stone bricks, brick+planks
	const bool logCorners = r.nextInt(10) < 7;
	const int floorT = r.nextInt(2) ? P : C;
	const int roofStyle = r.nextInt(3);           // flat, pyramid, gable
	const int windows = r.nextInt(onPlot ? 4 : 3); // none, holes, fence bars, (town) glass
	const int plankData = r.nextInt(3);            // oak, spruce, birch planks
	const int doorSide = r.nextInt(4);
	const int x0 = planX, y0 = planY, z0 = planZ;
	Planned p;
#define ADD(X, Y, Z, T, D) do { p.x = (X); p.y = (Y); p.z = (Z); p.tile = (T); p.data = (D); plan.push_back(p); } while (0)
	if (onPlot) {
		// terraforming first: the whole plot flat at the town's level, a layer at a time
		// from the top (like digging out a hill), then the holes filled from the bottom
		for (int yy = 6; yy >= 0; --yy) for (int xx = 0; xx < pw; ++xx) for (int zz = 0; zz < pd; ++zz) ADD(px0 + xx, y0 + yy, pz0 + zz, 0, 0);
		for (int yy = -3; yy <= -1; ++yy) for (int xx = 0; xx < pw; ++xx) for (int zz = 0; zz < pd; ++zz) ADD(px0 + xx, y0 + yy, pz0 + zz, Tile::dirt->id, -1);
	}
	// clear the inside (top layer first) and fill holes under the floor
	for (int yy = h + 4; yy >= 0; --yy) for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) ADD(x0 + xx, y0 + yy, z0 + zz, 0, 0);
	for (int xx = 0; xx < w; ++xx) for (int zz = 0; zz < d; ++zz) ADD(x0 + xx, y0 - 2, z0 + zz, Tile::dirt->id, -1); // only where there's nothing
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
			const int wx = x0 + xx, wz = z0 + zz;
			if (!edgeX && !edgeZ) {
				// second floor (with a stair hole along one wall)
				if (twoStory && yy == 3 && !(zz == 1 && xx >= 1 && xx <= 4)) ADD(wx, y0 + yy, wz, P, plankData);
				continue;
			}
			if (wx == doorX && wz == doorZ && yy < 2) continue;
			const bool corner = edgeX && edgeZ;
			// windows in the middle of each wall, one block up
			const bool mid = (edgeZ && !edgeX && (xx == w / 2 || xx == w / 2 - (w % 2 == 0))) || (edgeX && !edgeZ && (zz == d / 2 || zz == d / 2 - (d % 2 == 0)));
			if (windows && (yy == 1 || (twoStory && yy == 5)) && mid && !(wx == doorX || wz == doorZ)) {
				if (windows == 2) ADD(wx, y0 + yy, wz, Tile::fence->id, -2);
				if (windows == 3) ADD(wx, y0 + yy, wz, Tile::glass->id, -2);
				continue;
			}
			int t = P, dt = plankData;
			if (corner && logCorners) { t = L; dt = 0; }
			else if (wallStyle == 1) { t = C; dt = 0; }
			else if (wallStyle == 2 && yy == 0) { t = C; dt = 0; }
			else if (wallStyle == 3) { t = L; dt = 0; }
			else if (wallStyle == 4) { t = S; dt = 0; }
			else if (wallStyle == 5) { t = (yy < 3) ? S : P; dt = t == P ? plankData : 0; }
			ADD(wx, y0 + yy, wz, t, dt);
		}
	// stairs up to the second floor: full blocks along the wall
	if (twoStory) for (int k = 0; k < 3; ++k) ADD(x0 + 1 + k, y0 + k, z0 + 1, P, plankData);
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
	ADD(x0 + 1, y0, z0 + d - 2, Tile::torch->id, -2);
	ADD(x0 + w - 2, y0, z0 + d - 2, Tile::torch->id, -2);
	if (doorSide < 2) ADD(doorX + 1, y0 + 2, doorZ + (doorSide == 0 ? -1 : 1), Tile::torch->id, -2);
	// furniture: the table it uses at home, a furnace
	ADD(x0 + w - 2, y0, z0 + 1 + (twoStory ? 1 : 0), Tile::workBench->id, -2);
	ADD(x0 + w - 2, y0, z0 + d - 3 >= z0 + 2 ? z0 + d - 3 : z0 + d - 2, Tile::furnace->id, -2);
	if (twoStory) ADD(x0 + w - 2, y0 + 4, z0 + d - 2, Tile::torch->id, -2);
	if (onPlot) {
		// a path from the door to the road, and a lamp post
		int sx = doorX, sz = doorZ, dx = 0, dz = 0;
		if (doorSide == 0) dz = -1; else if (doorSide == 1) dz = 1; else if (doorSide == 2) dx = -1; else dx = 1;
		for (int k = 1; k < 8; ++k) {
			const int qx = sx + dx * k, qz = sz + dz * k;
			if (qx < px0 || qz < pz0 || qx >= px0 + pw || qz >= pz0 + pd) break;
			ADD(qx, y0 - 1, qz, Tile::gravel->id, 0);
		}
		ADD(px0, y0, pz0, Tile::fence->id, -2); ADD(px0, y0 + 1, pz0, Tile::fence->id, -2); ADD(px0, y0 + 2, pz0, Tile::torch->id, -2);
	}
#undef ADD
	// remember the house's table (it lives there; the carried one comes along)
	tableX = x0 + w - 2; tableY = y0; tableZ = z0 + 1 + (twoStory ? 1 : 0);
	hasTable = level->getTile(tableX, tableY, tableZ) == Tile::workBench->id;
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
		const int hgt = 5 + r.nextInt(2);
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
	else {
		if (!houseDone) say(SAY_LINES[T_BUILT][random.nextInt(6)], T_BUILT);
		houseDone = true;
		hasTable = level->getTile(tableX, tableY, tableZ) == Tile::workBench->id;
	}
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
			if (cur == 0 || cur == Tile::workBench->id || cur == Tile::chest->id || cur == Tile::furnace->id) continue;
			// clearing the plot: leave what somebody built there (roads, lamps, the player's stuff)
			const bool mine = p.x >= planX && p.x < planX + planW && p.z >= planZ && p.z < planZ + planD;
			if (!mine && NpcTown::artificial(cur)) continue;
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
		PlayerNPC* n = o[(i + random.nextInt((int)o.size())) % o.size()];
		if (!n->isBuilding() || n->distanceToSqr(this) > 64 * 64 || n->planPos >= n->plan.size()) continue;
		if (grudgeAgainst(n->number) >= 2) continue; // not helping a griefer
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
	if (inv[I_WHEAT] + ripe >= 3) push(Step::CRAFT, 0, 0, 0, 0, 0, 30); // bread (the 2x2 is enough here)
	return true;
}

bool PlayerNPC::planPen()
{
	// animals around?
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
