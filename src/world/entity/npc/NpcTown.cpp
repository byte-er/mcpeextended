#include "NpcTown.h"
#include "PlayerNPC.h"
#include "NpcChat.h"
#include "../EntityTypes.h"
#include "../../level/Level.h"
#include "../../level/storage/LevelData.h"
#include "../../level/tile/Tile.h"
#include "../../level/material/Material.h"
#include "../../level/tile/entity/ChestTileEntity.h"
#include "../../item/ItemInstance.h"
#include "../../../nbt/CompoundTag.h"
#include "../../../util/Mth.h"
#include "../../../util/Random.h"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <algorithm>

using namespace NpcChat;

namespace {
std::vector<NpcTown*> s_towns;

// where the ground is (under trees and plants; on top of water)
int terrainY(Level* level, int x, int z)
{
	int y = level->getHeightmap(x, z);
	if (y >= Level::DEPTH) y = Level::DEPTH - 1;
	while (y > 2) {
		const int t = level->getTile(x, y - 1, z);
		const Material* m = level->getMaterial(x, y - 1, z);
		if (t == 0 || Tile::isLeaf(t) || Tile::isLog(t) || (!m->blocksMotion() && !m->isLiquid())) { y--; continue; }
		break;
	}
	return y;
}

int ringOfCrossing(int k, int l) { return (std::max(std::abs(2 * k - 1), std::abs(2 * l - 1)) + 1) / 2; }
int ringOfPlot(int i, int j) { return std::max(std::abs(2 * i + 1), std::abs(2 * j + 1)) / 2 + 1; }
}

/** Blocks somebody put there: projects never knock these down (and the player's builds are safe). */
bool NpcTown::artificial(int t)
{
	if (t <= 0) return false;
	return t == Tile::wood->id || t == Tile::stoneBrick->id || t == Tile::stoneBrickSmooth->id || t == Tile::glass->id
		|| t == Tile::thinGlass->id || t == Tile::cloth->id || t == Tile::redBrick->id || t == Tile::bookshelf->id
		|| t == Tile::chest->id || t == Tile::workBench->id || t == Tile::furnace->id || t == Tile::furnace_lit->id
		|| t == Tile::fence->id || t == Tile::fenceGate->id || t == Tile::door_wood->id || t == Tile::door_iron->id
		|| t == Tile::torch->id || t == Tile::ladder->id || t == Tile::stairs_wood->id || t == Tile::stairs_stone->id
		|| t == Tile::stairs_brick->id || t == Tile::stairs_stoneBrickSmooth->id || t == Tile::stairs_sandStone->id
		|| t == Tile::stoneSlab->id || t == Tile::stoneSlabHalf->id || t == Tile::goldBlock->id || t == Tile::ironBlock->id
		|| t == Tile::emeraldBlock->id || t == Tile::lapisBlock->id || t == Tile::tnt->id || t == Tile::bed->id
		|| t == Tile::sign->id || t == Tile::wallSign->id || t == Tile::trapdoor->id || t == Tile::quartzBlock->id
		|| t == Tile::netherBrick->id || t == Tile::netherReactor->id || t == Tile::glowingObsidian->id
		|| t == Tile::crops->id || t == Tile::farmland->id || t == Tile::stonecutterBench->id;
}

NpcTown* NpcTown::get(Level* level)
{
	for (unsigned int i = 0; i < s_towns.size(); ++i) if (s_towns[i]->level == level) return s_towns[i];
	NpcTown* t = new NpcTown(level);
	s_towns.push_back(t);
	return t;
}

void NpcTown::levelGone(Level* level)
{
	for (unsigned int i = 0; i < s_towns.size(); ++i)
		if (s_towns[i]->level == level) { delete s_towns[i]; s_towns.erase(s_towns.begin() + i); return; }
}

NpcTown::NpcTown(Level* level)
:	active(false), phase(0), cells(2), founded(0), wx(0), wz(0), storageW(0), storageY(0), loaded(false),
	level(level), listTime(-1), listCount(-1), pathTime(-1), pathTokens(0), lastChat(-100000), chatLines(0),
	lastProjectCheck(-1), plannedFor(0)
{
}

// ---------------------------------------------------------------------------
// the NPC list (once per tick) and the shared budgets
// ---------------------------------------------------------------------------
const std::vector<PlayerNPC*>& NpcTown::npcs()
{
	const long now = level->getTime();
	if (now != listTime || (int)level->entities.size() != listCount) {
		listTime = now;
		listCount = (int)level->entities.size();
		list.clear();
		for (unsigned int i = 0; i < level->entities.size(); ++i) {
			Entity* e = level->entities[i];
			if (!e->removed && e->getEntityTypeId() == MobTypes::PlayerNPC) list.push_back((PlayerNPC*)e);
		}
	}
	return list;
}

int NpcTown::population()
{
	const std::vector<PlayerNPC*>& l = npcs();
	int n = 0;
	for (unsigned int i = 0; i < l.size(); ++i) if (!l[i]->removed && l[i]->isAlive()) n++;
	return n;
}

PlayerNPC* NpcTown::byNumber(int number)
{
	const std::vector<PlayerNPC*>& l = npcs();
	for (unsigned int i = 0; i < l.size(); ++i) if (l[i]->number == number && !l[i]->removed) return l[i];
	return NULL;
}

bool NpcTown::takePathToken()
{
	const long now = level->getTime();
	if (pathTime != now) { pathTime = now; pathTokens = 6; }
	if (pathTokens <= 0) return false;
	pathTokens--;
	return true;
}

bool NpcTown::mayChat(int importance)
{
	const int pop = population();
	if (pop < 10) return true;
	const long now = level->getTime();
	if (now - lastChat > 200 || now < lastChat) { lastChat = now; chatLines = 0; }
	const int cap = importance >= 2 ? 1000 : importance == 1 ? 4 : 2;
	if (chatLines >= cap) return false;
	if (importance == 0 && (level->random.nextInt(pop) >= 10)) return false;
	chatLines++;
	return true;
}

// ---------------------------------------------------------------------------
// founding and running the town
// ---------------------------------------------------------------------------
static int cellsFor(int pop)
{
	int c = 2;
	while ((2 * c) * (2 * c) < pop + 6 && c < 6) c++;
	return c;
}

void NpcTown::tick()
{
	const long now = level->getTime();
	if (now == lastProjectCheck) return;
	lastProjectCheck = now;
	npcs();
	if (!active) {
		if ((now % 40) == 0 && population() >= TOWN_MIN) found();
		return;
	}
	if ((now % 100) != 0) return;
	grow();
	for (unsigned int i = 0; i < projects.size(); ++i) projects[i].workers = 0;
	int good = 0, pop = 0;
	for (unsigned int i = 0; i < list.size(); ++i) {
		PlayerNPC* n = list[i];
		if (n->removed || !n->isAlive()) continue;
		pop++;
		if (n->goodTools()) good++;
		if (n->job == PlayerNPC::JOB_PROJECT && n->projectId >= 0 && n->projectId < (int)projects.size()) projects[n->projectId].workers++;
	}
	if (phase == 0) {
		// grind first: most people with stone tools and some stock in the chests (or it's been a while)
		const int stocked = storageReady() ? stock(Tile::stoneBrick->id) + stock(Tile::wood->id) + stock(Tile::treeTrunk->id) * 4 : 0;
		if ((good * 10 >= pop * 6 && stocked >= 128) || now - founded > 12000 || now < founded) {
			phase = 1;
			if (!list.empty()) {
				PlayerNPC* n = list[level->random.nextInt((int)list.size())];
				if (!n->removed) n->say(level->random.nextInt(2) ? "ok lets start building the town" : "time for some big projects", T_TOWN);
			}
		}
	}
}

void NpcTown::found()
{
	// where most of them are
	const std::vector<PlayerNPC*>& l = npcs();
	double sx = 0, sz = 0; int n = 0;
	for (unsigned int i = 0; i < l.size(); ++i) { if (l[i]->removed) continue; sx += l[i]->x; sz += l[i]->z; n++; }
	if (!n) return;
	const int ax = (int)(sx / n), az = (int)(sz / n);
	const int want = cellsFor(population());
	const int half = 6 * CELL + 8; // room for the biggest town
	int bestX = ax, bestZ = az; float best = 1e30f;
	Random r(level->getTime());
	for (int tries = 0; tries < 48; ++tries) {
		int x = ax, z = az;
		if (tries) { x = ax + r.nextInt(57) - 28; z = az + r.nextInt(57) - 28; }
		x = Mth::clamp(x, half, LEVEL_WIDTH - half); z = Mth::clamp(z, half, LEVEL_DEPTH - half);
		if (!level->isChunkLoaded(x >> 4, z >> 4)) continue;
		// flat, dry, and nobody's (the player's) buildings there
		const int h0 = terrainY(level, x, z);
		float score = 0;
		const int span = want * CELL;
		for (int k = 0; k < 40; ++k) {
			const int px = x + r.nextInt(2 * span + 1) - span, pz = z + r.nextInt(2 * span + 1) - span;
			if (!level->isChunkLoaded(px >> 4, pz >> 4)) { score += 4; continue; }
			const int h = terrainY(level, px, pz);
			score += std::abs(h - h0);
			if (level->getMaterial(px, h - 1, pz)->isLiquid()) score += 6;
			if (artificial(level->getTile(px, h - 1, pz)) || artificial(level->getTile(px, h, pz))) score += 200;
		}
		score += (std::abs(x - ax) + std::abs(z - az)) * 0.3f;
		if (score < best) { best = score; bestX = x; bestZ = z; }
	}
	wx = bestX + level->getLevelData()->originX * 16;
	wz = bestZ + level->getLevelData()->originZ * 16;
	active = true;
	loaded = true;
	phase = 0;
	founded = level->getTime();
	cells = want;
	heights.clear(); plotOwner.clear(); projects.clear(); griefs.clear();
	crossingHeight(0, 0);
	planProjects();
	if (getenv("MCPE_NPCLOG")) { printf("[npc] town founded at %d,%d (level %d,%d) cells %d\n", wx, wz, cx(), cz(), cells); fflush(stdout); }
	if (!l.empty()) {
		PlayerNPC* s = l[level->random.nextInt((int)l.size())];
		if (!s->removed) s->say(SAY_LINES[T_TOWN][level->random.nextInt(6)], T_TOWN);
	}
}

void NpcTown::grow()
{
	const int pop = population();
	const int want = cellsFor(pop);
	if (want > cells) ensureCells(want);
	else if ((pop >= 16 && plannedFor < 16) || (pop >= 30 && plannedFor < 30)) planProjects(); // towers, the wall
}

void NpcTown::ensureCells(int want)
{
	cells = Mth::Min(6, want);
	planProjects();
}

int NpcTown::cx() const { return wx - level->getLevelData()->originX * 16; }
int NpcTown::cz() const { return wz - level->getLevelData()->originZ * 16; }

int NpcTown::crossingHeight(int k, int l)
{
	const long key = cornerKey(k, l);
	std::map<long, int>::iterator it = heights.find(key);
	if (it != heights.end()) return it->second;
	const int x = cx() + k * CELL, z = cz() + l * CELL;
	int h;
	std::map<long, int>::iterator centre = heights.find(cornerKey(0, 0));
	if (x < 3 || z < 3 || x >= LEVEL_WIDTH - 3 || z >= LEVEL_DEPTH - 3 || !level->isChunkLoaded(x >> 4, z >> 4)) {
		h = centre != heights.end() ? centre->second : 64;
	} else {
		// a little average, so one tree stump or hole doesn't decide it
		int sum = 0, n = 0;
		for (int dx = -3; dx <= 3; dx += 3) for (int dz = -3; dz <= 3; dz += 3) { sum += terrainY(level, x + dx, z + dz); n++; }
		h = (sum + n / 2) / n;
	}
	// keep the roads walkable: close to the neighbours that are known already
	static const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	for (int i = 0; i < 4; ++i) {
		std::map<long, int>::iterator o = heights.find(cornerKey(k + nb[i][0], l + nb[i][1]));
		if (o != heights.end()) h = Mth::clamp(h, o->second - 4, o->second + 4);
	}
	if (centre != heights.end()) h = Mth::clamp(h, centre->second - 12, centre->second + 12);
	h = Mth::clamp(h, 6, Level::DEPTH - 20);
	heights[key] = h;
	return h;
}

int NpcTown::plotLevel(int i, int j)
{
	return (crossingHeight(i, j) + crossingHeight(i + 1, j) + crossingHeight(i, j + 1) + crossingHeight(i + 1, j + 1) + 2) / 4;
}

void NpcTown::plotRect(int i, int j, int& x0, int& z0, int& w, int& d) const
{
	x0 = cx() + i * CELL + 2; z0 = cz() + j * CELL + 2;
	w = CELL - 3; d = CELL - 3;
}

bool NpcTown::inTown(int x, int z, int margin) const
{
	if (!active) return false;
	const int r = cells * CELL + margin;
	return x >= cx() - r && x <= cx() + r && z >= cz() - r && z <= cz() + r;
}

bool NpcTown::onRoad(int x, int z, int margin) const
{
	if (!inTown(x, z, 2)) return false;
	int dx = (x - cx()) % CELL; if (dx < 0) dx += CELL;
	int dz = (z - cz()) % CELL; if (dz < 0) dz += CELL;
	return dx <= 1 + margin || dx >= CELL - 1 - margin || dz <= 1 + margin || dz >= CELL - 1 - margin;
}

bool NpcTown::inProjectArea(int x, int z, int margin)
{
	// the whole town (and the wall around it) is built on: mines, farms and the like go outside
	return inTown(x, z, margin + 2);
}

static bool isSpecialPlot(int i, int j, int cells)
{
	if ((i == 0 && j == 0) || (i == -1 && j == 0)) return true;
	if (i == cells - 1 && j == -cells) return true;  // the community farm(s)
	if (i == -cells && j == cells - 1 && cells >= 3) return true;
	return false;
}

bool NpcTown::plotFor(PlayerNPC* npc, int& oi, int& oj)
{
	if (!active) return false;
	for (std::map<long, int>::iterator it = plotOwner.begin(); it != plotOwner.end(); ++it)
		if (it->second == npc->number) { oi = (int)(it->first >> 16) - 4096; oj = (int)(it->first & 0xffff) - 4096; return true; }
	// nearest free plot to the middle that nobody's house is standing on
	int best = 1 << 30;
	bool found = false;
	for (int i = -cells; i < cells; ++i) for (int j = -cells; j < cells; ++j) {
		if (isSpecialPlot(i, j, cells) || plotOwner.count(cornerKey(i, j))) continue;
		const int ring = ringOfPlot(i, j);
		const int score = ring * 1000 + std::abs(i) * 10 + std::abs(j) * 10 + (i + j + 100) % 7;
		if (score >= best) continue;
		int x0, z0, w, d;
		plotRect(i, j, x0, z0, w, d);
		if (x0 < 4 || z0 < 4 || x0 + w >= LEVEL_WIDTH - 4 || z0 + d >= LEVEL_DEPTH - 4) continue;
		bool taken = false;
		const std::vector<PlayerNPC*>& l = npcs();
		for (unsigned int k = 0; k < l.size() && !taken; ++k) {
			const PlayerNPC* o = l[k];
			if (o == npc || o->removed || o->planW <= 0) continue;
			if (o->planX < x0 + w && o->planX + o->planW > x0 && o->planZ < z0 + d && o->planZ + o->planD > z0) taken = true;
		}
		if (taken) continue;
		best = score; oi = i; oj = j; found = true;
	}
	if (!found) return false;
	plotOwner[cornerKey(oi, oj)] = npc->number;
	return true;
}

void NpcTown::addProject(int type, int i, int j, int dir)
{
	for (unsigned int k = 0; k < projects.size(); ++k)
		if (projects[k].type == type && projects[k].i == i && projects[k].j == j && projects[k].dir == dir) return;
	NpcProject p;
	p.type = type; p.i = i; p.j = j; p.dir = dir;
	p.seed = (int)(wx * 31 + wz * 17 + i * 1013 + j * 7919 + type * 104729 + dir);
	p.pos = 0; p.passes = 0; p.done = false; p.workers = 0; p.built = false;
	projects.push_back(p);
}

void NpcTown::planProjects()
{
	plannedFor = population();
	addProject(NpcProject::STORAGE, 0, 0, 0);
	addProject(NpcProject::PLAZA, 0, 0, 0);
	const int pop = population();
	for (int ring = 1; ring <= cells + 1; ++ring) {
		for (int k = -cells; k <= cells; ++k) for (int l = -cells; l <= cells; ++l) {
			if (k + 1 <= cells && std::max(ringOfCrossing(k, l), ringOfCrossing(k + 1, l)) == ring) addProject(NpcProject::ROAD, k, l, 0);
			if (l + 1 <= cells && std::max(ringOfCrossing(k, l), ringOfCrossing(k, l + 1)) == ring) addProject(NpcProject::ROAD, k, l, 1);
		}
		if (ring == 1) addProject(NpcProject::HALL, -1, 0, 0);
		if (ring == 2) {
			addProject(NpcProject::FARM, cells - 1, -cells, 0);
			if (cells >= 3) addProject(NpcProject::FARM, -cells, cells - 1, 0);
		}
		// empty plots of this ring get levelled
		if (ring >= 2)
			for (int i = -cells; i < cells; ++i) for (int j = -cells; j < cells; ++j)
				if (ringOfPlot(i, j) == ring && !isSpecialPlot(i, j, cells)) addProject(NpcProject::LEVEL, i, j, 0);
	}
	if (pop >= 16) for (int c = 0; c < 4; ++c) addProject(NpcProject::TOWER, c, cells, 0);
	if (pop >= 30) for (int side = 0; side < 4; ++side) for (int k = -cells; k < cells; ++k) addProject(NpcProject::WALL, k, cells, side);
}

NpcProject* NpcTown::pickProject(PlayerNPC* npc)
{
	if (!active) return NULL;
	const int limit = Mth::clamp(population() / 5, 2, 12);
	NpcProject* best = NULL;
	float bestScore = 1e30f;
	int open = 0;
	for (unsigned int i = 0; i < projects.size(); ++i) {
		NpcProject& p = projects[i];
		if (p.done) continue;
		if (phase == 0 && p.type != NpcProject::STORAGE) continue;
		if (++open > limit) break;
		// a plot someone owns is their business (LEVEL only does the empty ones)
		if (p.type == NpcProject::LEVEL && plotOwner.count(cornerKey(p.i, p.j))) { p.done = true; continue; }
		if (p.workers >= 8) continue;
		int px = cx() + p.i * CELL + CELL / 2, pz = cz() + p.j * CELL + CELL / 2;
		if (p.type == NpcProject::ROAD) { px = cx() + p.i * CELL + (p.dir == 0 ? CELL / 2 : 0); pz = cz() + p.j * CELL + (p.dir == 1 ? CELL / 2 : 0); }
		const float dist = Mth::sqrt((px - npc->x) * (px - npc->x) + (pz - npc->z) * (pz - npc->z));
		float score = p.workers * 12.0f + dist / 8.0f + open * 2.0f;
		if (npc->role == PlayerNPC::ROLE_FARMER && p.type == NpcProject::FARM) score -= 30;
		if (npc->role == PlayerNPC::ROLE_BUILDER && (p.type == NpcProject::HALL || p.type == NpcProject::PLAZA)) score -= 8;
		if (score < bestScore) { bestScore = score; best = &p; }
	}
	return best;
}

bool NpcTown::claimBlock(NpcProject* p, NpcPlanned& out)
{
	if (!p->built) buildPlan(*p);
	const std::vector<PlayerNPC*>& l = npcs();
	while (true) {
		if (p->pos >= p->plan.size()) {
			if (p->passes >= 1 || p->plan.empty()) {
				p->done = true;
				std::vector<NpcPlanned>().swap(p->plan);
				return false;
			}
			p->passes++;
			p->pos = 0;
		}
		const NpcPlanned& b = p->plan[p->pos++];
		if (b.x < 2 || b.z < 2 || b.x >= LEVEL_WIDTH - 2 || b.z >= LEVEL_DEPTH - 2 || b.y < 2 || b.y >= Level::DEPTH - 2) continue;
		if (!level->isChunkLoaded(b.x >> 4, b.z >> 4)) continue;
		const int cur = level->getTile(b.x, b.y, b.z);
		const bool liquid = cur != 0 && level->getMaterial(b.x, b.y, b.z)->isLiquid();
		// never through somebody's house, farm or pen
		bool theirs = false;
		for (unsigned int k = 0; k < l.size() && !theirs; ++k) {
			const PlayerNPC* o = l[k];
			if (o->removed) continue;
			if (o->planW > 0 && b.x >= o->planX - 1 && b.x <= o->planX + o->planW && b.z >= o->planZ - 1 && b.z <= o->planZ + o->planD) theirs = true;
			if (o->farmW > 0 && b.x >= o->farmX - 1 && b.x <= o->farmX + o->farmW && b.z >= o->farmZ - 1 && b.z <= o->farmZ + o->farmD) theirs = true;
			if (o->penW > 0 && b.x >= o->penX - 1 && b.x <= o->penX + o->penW && b.z >= o->penZ - 1 && b.z <= o->penZ + o->penD) theirs = true;
		}
		if (theirs) continue;
		if (b.tile == 0) {
			if (cur == 0 || liquid || artificial(cur)) continue;
			out = b; return true;
		}
		if (b.tile == Tile::farmland->id) {
			if (cur != Tile::dirt->id && cur != ((Tile*)Tile::grass)->id) continue;
			if (level->getTile(b.x, b.y + 1, b.z) != 0) continue;
			out = b; return true;
		}
		if (cur == b.tile) continue;
		if (b.data == -1 && cur != 0 && !liquid) continue; // fill only
		if (b.data == -3 && (cur == Tile::dirt->id || cur == ((Tile*)Tile::grass)->id || cur == Tile::farmland->id)) continue;
		if (cur != 0 && !liquid && artificial(cur)) continue; // something somebody built is in the way: leave it
		out = b;
		if (out.data == -3) out.data = 0;
		return true;
	}
}

// ---------------------------------------------------------------------------
// the plans (made from the project's numbers, so they come out the same every time)
// ---------------------------------------------------------------------------
void NpcTown::buildPlan(NpcProject& p)
{
	p.plan.clear();
	p.built = true;
	std::vector<NpcPlanned>& out = p.plan;
	NpcPlanned b;
#define ADD(X, Y, Z, T, D) do { b.x = (X); b.y = (Y); b.z = (Z); b.tile = (T); b.data = (D); out.push_back(b); } while (0)
	const int AIR = 0, P = Tile::wood->id, C = Tile::stoneBrick->id, S = Tile::stoneBrickSmooth->id, L = Tile::treeTrunk->id;
	const int F = Tile::fence->id, T = Tile::torch->id, G = Tile::gravel->id, D = Tile::dirt->id;
	Random r(p.seed);
	switch (p.type) {
	case NpcProject::STORAGE: {
		int x0, z0, w, d;
		plotRect(0, 0, x0, z0, w, d);
		const int y = plotLevel(0, 0);
		for (int x = x0; x < x0 + w; ++x) { ADD(x, y + 1, z0 + 1, AIR, 0); ADD(x, y, z0 + 1, AIR, 0); ADD(x, y - 1, z0 + 1, D, -1); }
		for (int k = 0; k < storageCount(); ++k) { int sx, sy, sz; storagePos(k, sx, sy, sz); ADD(sx, sy, sz, Tile::chest->id, 3); }
		ADD(x0 + 1, y, z0 + 1, Tile::workBench->id, 0);
		ADD(x0 + w - 2, y, z0 + 1, Tile::furnace->id, 3);
		ADD(x0 + w - 1, y, z0 + 1, Tile::furnace->id, 3);
		break;
	}
	case NpcProject::PLAZA: {
		int x0, z0, w, d;
		plotRect(0, 0, x0, z0, w, d);
		const int y = plotLevel(0, 0);
		const int mx = x0 + w / 2, mz = z0 + d / 2;
		for (int yy = 6; yy >= 0; --yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, AIR, 0);
		for (int yy = -4; yy <= -2; ++yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, D, -1);
		// the square: a stone brick checkerboard with a cobble border, a well in the middle
		for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) {
			if (std::abs(x - mx) <= 1 && std::abs(z - mz) <= 1) continue;
			const bool edge = x == x0 || z == z0 || x == x0 + w - 1 || z == z0 + d - 1;
			ADD(x, y - 1, z, edge ? C : (((x + z) & 1) ? S : C), 0);
		}
		ADD(mx, y - 2, mz, C, 0);
		ADD(mx, y - 1, mz, Tile::calmWater->id, 0);
		for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) {
			if (!dx && !dz) continue;
			ADD(mx + dx, y - 1, mz + dz, C, 0);
			ADD(mx + dx, y, mz + dz, C, 0);
		}
		for (int c = 0; c < 4; ++c) {
			const int fx = mx + (c & 1 ? 1 : -1), fz = mz + (c & 2 ? 1 : -1);
			ADD(fx, y + 1, fz, F, 0); ADD(fx, y + 2, fz, F, 0);
		}
		for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) ADD(mx + dx, y + 3, mz + dz, Tile::stoneSlabHalf->id, 0);
		// lamp posts in the corners
		for (int c = 0; c < 4; ++c) {
			const int lx = c & 1 ? x0 + w - 1 : x0, lz = c & 2 ? z0 + d - 1 : z0 + 2;
			ADD(lx, y, lz, F, 0); ADD(lx, y + 1, lz, F, 0); ADD(lx, y + 2, lz, T, -2);
		}
		break;
	}
	case NpcProject::ROAD: {
		const int k = p.i, l = p.j;
		const int hA = crossingHeight(k, l), hB = crossingHeight(p.dir == 0 ? k + 1 : k, p.dir == 0 ? l : l + 1);
		const int bx = cx() + k * CELL, bz = cz() + l * CELL;
		for (int t = 0; t <= CELL; ++t) {
			const int y = (hA * (CELL - t) + hB * t + CELL / 2) / CELL;
			for (int s = -2; s <= 2; ++s) {
				const int x = p.dir == 0 ? bx + t : bx + s, z = p.dir == 0 ? bz + s : bz + t;
				for (int yy = 5; yy >= 0; --yy) ADD(x, y + yy, z, AIR, 0);
			}
			for (int s = -1; s <= 1; ++s) {
				const int x = p.dir == 0 ? bx + t : bx + s, z = p.dir == 0 ? bz + s : bz + t;
				for (int yy = -4; yy <= -2; ++yy) ADD(x, y + yy, z, D, -1);
				ADD(x, y - 1, z, s == 0 ? G : C, 0);
			}
			if (t == CELL / 2) {
				const int x = p.dir == 0 ? bx + t : bx + 2, z = p.dir == 0 ? bz + 2 : bz + t;
				ADD(x, y, z, F, -2); ADD(x, y + 1, z, F, -2); ADD(x, y + 2, z, T, -2);
			}
		}
		break;
	}
	case NpcProject::HALL: {
		int x0, z0, w, d;
		plotRect(p.i, p.j, x0, z0, w, d);
		const int y = plotLevel(p.i, p.j);
		for (int yy = 10; yy >= 0; --yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, AIR, 0);
		for (int yy = -4; yy <= -2; ++yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, D, -1);
		const int bx0 = x0 + 1, bz0 = z0 + 1, bw = w - 2, bd = d - 2;
		for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) {
			const bool inside = x > bx0 && x < bx0 + bw - 1 && z > bz0 && z < bz0 + bd - 1;
			ADD(x, y - 1, z, inside ? P : C, inside ? 1 : 0);
		}
		const int doorZ = bz0 + bd / 2;
		for (int yy = 0; yy < 8; ++yy)
			for (int x = bx0; x < bx0 + bw; ++x) for (int z = bz0; z < bz0 + bd; ++z) {
				const bool ex = x == bx0 || x == bx0 + bw - 1, ez = z == bz0 || z == bz0 + bd - 1;
				if (!ex && !ez) {
					if (yy == 4 && !(z == bz0 + 1 && x <= bx0 + 5)) ADD(x, y + 4, z, P, 1); // upper floor, stair hole
					continue;
				}
				if (x == bx0 + bw - 1 && (z == doorZ || z == doorZ - 1) && yy < 2) continue; // double doorway to the plaza
				const bool corner = ex && ez;
				const int along = ex ? z - bz0 : x - bx0;
				const bool window = !corner && (yy == 1 || yy == 2 || yy == 5 || yy == 6) && (along % 3) == 1;
				if (window) { ADD(x, y + yy, z, Tile::glass->id, 0); continue; }
				ADD(x, y + yy, z, corner ? L : (yy == 0 || yy == 4) ? C : S, 0);
			}
		ADD(bx0 + bw - 1, y, doorZ, Tile::door_wood->id, 2 | 4);
		ADD(bx0 + bw - 1, y, doorZ - 1, Tile::door_wood->id, 2 | 4);
		for (int k = 0; k < 4; ++k) ADD(bx0 + 1 + k, y + k, bz0 + 1, P, 1); // stairs up
		// flat roof with battlements
		for (int x = bx0 - 1; x <= bx0 + bw; ++x) for (int z = bz0 - 1; z <= bz0 + bd; ++z) {
			const bool rim = x == bx0 - 1 || z == bz0 - 1 || x == bx0 + bw || z == bz0 + bd;
			ADD(x, y + 8, z, rim ? S : P, rim ? 0 : 1);
			if (rim && ((x + z) & 1)) ADD(x, y + 9, z, S, 0);
		}
		// inside: lights, a table, furnaces, books upstairs
		ADD(bx0 + 1, y + 1, bz0 + bd - 2, T, -2); ADD(bx0 + bw - 2, y + 1, bz0 + 1, T, -2);
		ADD(bx0 + bw - 2, y + 1, bz0 + bd - 2, T, -2);
		ADD(bx0 + 1, y, bz0 + bd - 2, Tile::workBench->id, -2);
		ADD(bx0 + 2, y, bz0 + bd - 2, Tile::furnace->id, -2);
		for (int x = bx0 + 2; x < bx0 + bw - 2; ++x) ADD(x, y + 5, bz0 + bd - 2, Tile::bookshelf->id, -2);
		ADD(bx0 + 1, y + 6, bz0 + bd - 2, T, -2); ADD(bx0 + bw - 2, y + 6, bz0 + 2, T, -2);
		ADD(bx0 + bw / 2, y + 9, bz0 + bd / 2, T, -2);
		break;
	}
	case NpcProject::FARM: {
		int x0, z0, w, d;
		plotRect(p.i, p.j, x0, z0, w, d);
		const int y = plotLevel(p.i, p.j);
		for (int yy = 6; yy >= 0; --yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, AIR, 0);
		for (int yy = -4; yy <= -2; ++yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, D, -1);
		const int gateX = p.i >= 0 ? x0 : x0 + w - 1, gateZ = z0 + d / 2;
		for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) {
			const bool edge = x == x0 || z == z0 || x == x0 + w - 1 || z == z0 + d - 1;
			if (edge) { ADD(x, y - 1, z, D, -1); ADD(x, y, z, (x == gateX && z == gateZ) ? Tile::fenceGate->id : F, 0); continue; }
			const bool water = (x - x0 == 3 || x - x0 == w - 4) && (z - z0 == 3 || z - z0 == d - 4);
			if (water) { ADD(x, y - 1, z, Tile::calmWater->id, 0); continue; }
			ADD(x, y - 1, z, D, -3);                       // dirt (grass is fine too)
			ADD(x, y - 1, z, Tile::farmland->id, 0);       // till
			ADD(x, y, z, Tile::crops->id, 0);              // plant
		}
		for (int c = 0; c < 4; ++c) ADD(c & 1 ? x0 + w - 1 : x0, y + 1, c & 2 ? z0 + d - 1 : z0, T, -2);
		break;
	}
	case NpcProject::TOWER: {
		const int c = p.i;
		const int kx = (c & 1) ? cells : -cells, lz = (c & 2) ? cells : -cells;
		const int y = crossingHeight(kx, lz);
		const int tx = cx() + kx * CELL + ((c & 1) ? 4 : -8), tz = cz() + lz * CELL + ((c & 2) ? 4 : -8);
		for (int yy = 7; yy >= 0; --yy) for (int x = tx - 1; x < tx + 6; ++x) for (int z = tz - 1; z < tz + 6; ++z) ADD(x, y + yy, z, AIR, 0);
		for (int yy = -4; yy <= -1; ++yy) for (int x = tx; x < tx + 5; ++x) for (int z = tz; z < tz + 5; ++z) ADD(x, y + yy, z, yy == -1 ? C : D, yy == -1 ? 0 : -1);
		const int doorX = tx + 2, doorZ = (c & 2) ? tz : tz + 4;
		for (int yy = 0; yy < 5; ++yy) for (int x = tx; x < tx + 5; ++x) for (int z = tz; z < tz + 5; ++z) {
			const bool ex = x == tx || x == tx + 4, ez = z == tz || z == tz + 4;
			if (!ex && !ez) continue;
			if (x == doorX && z == doorZ && yy < 2) continue;
			ADD(x, y + yy, z, (ex && ez) ? L : C, 0);
		}
		for (int x = tx - 1; x <= tx + 5; ++x) for (int z = tz - 1; z <= tz + 5; ++z) ADD(x, y + 5, z, P, 0);
		for (int x = tx - 1; x <= tx + 5; ++x) for (int z = tz - 1; z <= tz + 5; ++z)
			if (x == tx - 1 || z == tz - 1 || x == tx + 5 || z == tz + 5) ADD(x, y + 6, z, F, -2);
		ADD(tx - 1, y + 7, tz - 1, T, -2); ADD(tx + 5, y + 7, tz - 1, T, -2); ADD(tx - 1, y + 7, tz + 5, T, -2); ADD(tx + 5, y + 7, tz + 5, T, -2);
		ADD(tx + 2, y, tz + 2, T, -2);
		break;
	}
	case NpcProject::WALL: {
		// one stretch of the town wall, with gates where the roads come out
		const int side = p.dir, k = p.i;
		const int edge = cells * CELL + 4;
		const bool alongX = side < 2;
		const int fixed = alongX ? (side == 0 ? cz() - edge : cz() + edge) : (side == 2 ? cx() - edge : cx() + edge);
		const int hA = alongX ? crossingHeight(k, side == 0 ? -cells : cells) : crossingHeight(side == 2 ? -cells : cells, k);
		const int hB = alongX ? crossingHeight(k + 1, side == 0 ? -cells : cells) : crossingHeight(side == 2 ? -cells : cells, k + 1);
		const int t0 = k == -cells ? -4 : 0, t1 = k == cells - 1 ? CELL + 4 : CELL - 1;
		for (int t = t0; t <= t1; ++t) {
			const int tc = Mth::clamp(t, 0, CELL);
			const int y = (hA * (CELL - tc) + hB * tc + CELL / 2) / CELL;
			const int along = (alongX ? cx() : cz()) + k * CELL + t;
			const int x = alongX ? along : fixed, z = alongX ? fixed : along;
			int off = (along - (alongX ? cx() : cz())) % CELL; if (off < 0) off += CELL;
			const bool gate = (off <= 1 || off >= CELL - 1) && std::abs(t) < CELL + 3 && !(k == -cells && t < 0) && !(k == cells - 1 && t > CELL);
			for (int yy = -3; yy <= -1; ++yy) ADD(x, y + yy, z, D, -1);
			if (gate) { for (int yy = 3; yy >= 0; --yy) ADD(x, y + yy, z, AIR, 0); continue; }
			ADD(x, y, z, C, 0); ADD(x, y + 1, z, S, 0); ADD(x, y + 2, z, C, 0);
			if ((t & 1) == 0) ADD(x, y + 3, z, C, 0);
			if (off == 2 || off == CELL - 2) ADD(x, y + 4, z, T, -2);
		}
		break;
	}
	case NpcProject::LEVEL: {
		int x0, z0, w, d;
		plotRect(p.i, p.j, x0, z0, w, d);
		const int y = plotLevel(p.i, p.j);
		for (int yy = 6; yy >= 0; --yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, AIR, 0);
		for (int yy = -4; yy <= -1; ++yy) for (int x = x0; x < x0 + w; ++x) for (int z = z0; z < z0 + d; ++z) ADD(x, y + yy, z, D, -1);
		break;
	}
	}
#undef ADD
}

// ---------------------------------------------------------------------------
// storage chests
// ---------------------------------------------------------------------------
void NpcTown::storagePos(int k, int& x, int& y, int& z)
{
	int x0, z0, w, d;
	plotRect(0, 0, x0, z0, w, d);
	x = x0 + 3 + k * 2; y = plotLevel(0, 0); z = z0 + 1;
}

ChestTileEntity* NpcTown::chest(int k)
{
	int x, y, z;
	storagePos(k, x, y, z);
	if (x < 1 || z < 1 || x >= LEVEL_WIDTH - 1 || z >= LEVEL_DEPTH - 1 || level->getTile(x, y, z) != Tile::chest->id) return NULL;
	TileEntity* te = level->getTileEntity(x, y, z);
	return te ? static_cast<ChestTileEntity*>(te) : NULL;
}

bool NpcTown::storageReady()
{
	if (!active) return false;
	for (int k = 0; k < storageCount(); ++k) if (chest(k)) return true;
	return false;
}

int NpcTown::stock(int id, int aux)
{
	int n = 0;
	for (int k = 0; k < storageCount(); ++k) {
		ChestTileEntity* c = chest(k);
		if (!c) continue;
		for (int s = 0; s < c->getContainerSize(); ++s) {
			ItemInstance* it = c->getItem(s);
			if (it && !it->isNull() && it->count > 0 && it->id == id && (aux < 0 || it->getAuxValue() == aux)) n += it->count;
		}
	}
	return n;
}

int NpcTown::deposit(int id, int aux, int count)
{
	int added = 0;
	for (int k = 0; k < storageCount() && count > 0; ++k) {
		ChestTileEntity* c = chest(k);
		if (!c) continue;
		while (count > 0) {
			const int n = Mth::Min(count, 64);
			ItemInstance tmp(id, n, aux);
			c->add(&tmp);
			const int put = n - tmp.count;
			added += put; count -= put;
			if (put < n) break; // this one's full
		}
		c->setChanged();
	}
	return added;
}

int NpcTown::withdraw(int id, int aux, int count)
{
	int taken = 0;
	for (int k = 0; k < storageCount() && count > 0; ++k) {
		ChestTileEntity* c = chest(k);
		if (!c) continue;
		for (int s = 0; s < c->getContainerSize() && count > 0; ++s) {
			ItemInstance* it = c->getItem(s);
			if (!it || it->isNull() || it->count <= 0 || it->id != id || (aux >= 0 && it->getAuxValue() != aux)) continue;
			const int n = Mth::Min(count, it->count);
			it->count -= n; taken += n; count -= n;
			if (it->count <= 0) c->clearSlot(s);
		}
		c->setChanged();
	}
	return taken;
}

// ---------------------------------------------------------------------------
// griefing: the victim and whoever saw it find out
// ---------------------------------------------------------------------------
void NpcTown::reportGrief(PlayerNPC* griefer, int victim, int x, int y, int z)
{
	Grief g = { griefer->number, victim, x, y, z, level->getTime() };
	if (griefs.size() >= 32) griefs.erase(griefs.begin());
	griefs.push_back(g);
	const std::vector<PlayerNPC*>& l = npcs();
	for (unsigned int i = 0; i < l.size(); ++i) {
		PlayerNPC* n = l[i];
		if (n == griefer || n->removed || !n->isAlive()) continue;
		const float dx = n->x - x, dz = n->z - z;
		const bool saw = dx * dx + dz * dz < 20 * 20;
		if (n->number == victim || saw) n->heardGrief(griefer, victim, x, y, z, saw);
	}
	if (getenv("MCPE_NPCLOG")) { printf("[npc] %s griefed %d at %d,%d,%d\n", griefer->getNpcName().c_str(), victim, x, y, z); fflush(stdout); }
}

// ---------------------------------------------------------------------------
// saving (with every NPC; the first one loaded brings it back)
// ---------------------------------------------------------------------------
void NpcTown::save(CompoundTag* tag)
{
	tag->putByte("Active", active);
	tag->putInt("Phase", phase);
	tag->putInt("Cells", cells);
	tag->putInt("Founded", (int)founded);
	tag->putInt("WX", wx); tag->putInt("WZ", wz);
	std::string s;
	char buf[96];
	for (std::map<long, int>::iterator it = heights.begin(); it != heights.end(); ++it) {
		sprintf(buf, "%d,%d,%d;", (int)(it->first >> 16) - 4096, (int)(it->first & 0xffff) - 4096, it->second); s += buf;
	}
	tag->putString("Heights", s);
	s.clear();
	for (std::map<long, int>::iterator it = plotOwner.begin(); it != plotOwner.end(); ++it) {
		sprintf(buf, "%d,%d,%d;", (int)(it->first >> 16) - 4096, (int)(it->first & 0xffff) - 4096, it->second); s += buf;
	}
	tag->putString("Plots", s);
	s.clear();
	for (unsigned int i = 0; i < projects.size(); ++i) {
		const NpcProject& p = projects[i];
		sprintf(buf, "%d,%d,%d,%d,%d,%d,%d;", p.type, p.i, p.j, p.dir, p.done ? -1 : (int)p.pos, p.passes, p.seed); s += buf;
	}
	tag->putString("Projects", s);
}

void NpcTown::load(CompoundTag* tag)
{
	loaded = true;
	active = tag->getByte("Active") != 0;
	if (!active) return;
	phase = tag->getInt("Phase");
	cells = Mth::clamp(tag->getInt("Cells"), 2, 6);
	founded = tag->getInt("Founded");
	wx = tag->getInt("WX"); wz = tag->getInt("WZ");
	heights.clear(); plotOwner.clear(); projects.clear();
	std::string s = tag->getString("Heights");
	for (size_t p = 0; p < s.size();) {
		int k, l, h;
		if (sscanf(s.c_str() + p, "%d,%d,%d", &k, &l, &h) == 3) heights[cornerKey(k, l)] = h;
		const size_t e = s.find(';', p);
		if (e == std::string::npos) break;
		p = e + 1;
	}
	s = tag->getString("Plots");
	for (size_t p = 0; p < s.size();) {
		int i, j, n;
		if (sscanf(s.c_str() + p, "%d,%d,%d", &i, &j, &n) == 3) plotOwner[cornerKey(i, j)] = n;
		const size_t e = s.find(';', p);
		if (e == std::string::npos) break;
		p = e + 1;
	}
	s = tag->getString("Projects");
	for (size_t p = 0; p < s.size();) {
		int type, i, j, dir, pos, passes, seed;
		if (sscanf(s.c_str() + p, "%d,%d,%d,%d,%d,%d,%d", &type, &i, &j, &dir, &pos, &passes, &seed) == 7) {
			NpcProject pr;
			pr.type = type; pr.i = i; pr.j = j; pr.dir = dir; pr.seed = seed;
			pr.done = pos < 0; pr.pos = pos < 0 ? 0 : 0; // (blocks already there are skipped)
			pr.passes = passes; pr.workers = 0; pr.built = false;
			projects.push_back(pr);
		}
		const size_t e = s.find(';', p);
		if (e == std::string::npos) break;
		p = e + 1;
	}
	planProjects(); // anything missing
}
