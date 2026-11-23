// Player NPCs: carrying out the planned steps (walking, digging, placing, chests).
#include "PlayerNPC.h"
#include "NpcChat.h"
#include "../EntityTypes.h"
#include "../../level/Level.h"
#include "../../level/tile/Tile.h"
#include "../../level/tile/LevelEvent.h"
#include "../../level/material/Material.h"
#include "../../item/Item.h"
#include "../../phys/Vec3.h"
#include "../../phys/AABB.h"
#include "../../../util/Mth.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace NpcChat;

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

void PlayerNPC::push(Step::Kind k, int x, int y, int z, int tile, int data, int ticks)
{
	Step s;
	s.kind = k; s.x = x; s.y = y; s.z = z; s.tile = tile; s.data = data; s.ticks = ticks; s.proj = -1;
	steps.push_back(s);
}

void PlayerNPC::pushFront(Step::Kind k, int x, int y, int z, int tile, int data, int ticks)
{
	Step s;
	s.kind = k; s.x = x; s.y = y; s.z = z; s.tile = tile; s.data = data; s.ticks = ticks; s.proj = -1;
	steps.push_front(s);
}

// building things a bit taller than a player can reach without pillaring up
float PlayerNPC::reach() const
{
	return (job == JOB_BUILD || job == JOB_EXTRA || job == JOB_HELP || job == JOB_PROJECT) ? 5.4f : 4.4f;
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

// 1 arrived, 0 walking
bool PlayerNPC::walkTo(float tx, float ty, float tz, float reach)
{
	const float dx = tx + 0.5f - x, dz = tz + 0.5f - z, dy = ty - (y - heightOffset);
	const float d2 = dx * dx + dz * dz;
	if (d2 <= reach * reach && std::fabs(dy) < 1.6f) { route.destroy(); directTicks = 0; return true; }
	sprinting = d2 > 14 * 14 && !isInWater();
	// no closer after a while: dig through what's in front (or give up eventually)
	if (progDist >= 1e8f) { progDist = Mth::sqrt(d2) + std::fabs(dy); progTimer = 0; }
	if (++progTimer >= 60) {
		progTimer = 0;
		const float dist = Mth::sqrt(d2) + std::fabs(dy);
		if (progDist - dist < 1.0f && progDist < 1e8f) {
			routeFails++;
			const float ang = (float)Mth::atan2(dz, dx);
			const bool climb = dy > 1.5f && (inv[I_DIRT] + inv[I_COBBLE] + inv[I_PLANK]) <= 0;
			if (dy > 1.5f && (inv[I_DIRT] + inv[I_COBBLE] + inv[I_PLANK]) > 0) {
				// down in a hole: jump and put a block under me, like anyone would
				Step p; p.kind = Step::PILLAR; p.x = Mth::floor(x); p.y = Mth::floor(y - heightOffset); p.z = Mth::floor(z); p.tile = 0; p.data = 0; p.ticks = 0; p.proj = -1;
				digQueue.push_back(p);
			}
			const int fx = Mth::floor(x + Mth::cos(ang) * 0.9f), fz = Mth::floor(z + Mth::sin(ang) * 0.9f), fy = Mth::floor(y - heightOffset);
			// the target is below: a staircase down (one step forward, one down);
			// above and nothing to pillar with: a staircase up (the block in front stays as the step)
			const int lowest = climb ? 1 : (dy < -1.5f && job == JOB_CAVE) ? -1 : 0;
			const int highest = climb ? 2 : 1;
			for (int k = highest; k >= lowest; --k) {
				const int t = level->getTile(fx, fy + k, fz);
				if (t != 0 && level->getMaterial(fx, fy + k, fz)->blocksMotion() && t != Tile::unbreakable->id && t != Tile::invisible_bedrock->id && t != Tile::chest->id) {
					Step b; b.kind = Step::BREAK; b.x = fx; b.y = fy + k; b.z = fz; b.tile = -1; b.data = 0; b.ticks = 0; b.proj = -1;
					digQueue.push_back(b);
				}
			}
			// a block over my head keeps me from jumping out
			const int up = level->getTile(Mth::floor(x), fy + 2, Mth::floor(z));
			if (up != 0 && level->getMaterial(Mth::floor(x), fy + 2, Mth::floor(z))->blocksMotion() && up != Tile::unbreakable->id && up != Tile::chest->id) {
				Step b; b.kind = Step::BREAK; b.x = Mth::floor(x); b.y = fy + 2; b.z = Mth::floor(z); b.tile = -1; b.data = 0; b.ticks = 0; b.proj = -1;
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
		// lots of NPCs: only so many path searches per tick, the rest keep going straight a moment
		if (!NpcTown::get(level)->takePathToken()) {
			if (!route.isEmpty()) { followRoute(); return false; }
			yRot = (float)(Mth::atan2(dz, dx) * 180 / Mth::PI) - 90;
			yya = runSpeed * 0.6f;
			if (horizontalCollision) jumping = true;
			return false;
		}
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
	yBodyRot += (yRot - yBodyRot) * 0.5f; // the body follows the head a little later, like a player
}

static int toolSpeed(int tier) { return tier == 4 ? 8 : tier == 3 ? 6 : tier == 2 ? 4 : tier == 1 ? 2 : 1; }

int PlayerNPC::digTicks(int t)
{
	Tile* tile = Tile::tiles[t];
	if (!tile || tile->destroySpeed < 0) return -1;
	float sec = tile->destroySpeed * 1.5f;
	const Material* m = tile->material;
	if (m == Material::stone) {
		if (pick == 0) sec *= 3.3f;
		else sec /= toolSpeed(pick);
	} else if (m == Material::wood) {
		if (axe) sec /= toolSpeed(axe);
	} else if (m == Material::dirt || m == Material::sand || m == Material::snow || m == Material::clay) {
		if (pick >= 2) sec /= 2; // (a shovel, more or less)
	}
	return Mth::Max(3, (int)(sec * 20));
}

// ores drop only with a good enough pickaxe, like for a player
bool PlayerNPC::canHarvest(int t) const
{
	if (t == Tile::ironOre->id || t == Tile::lapisOre->id) return pick >= 2;
	if (t == Tile::goldOre->id || t == Tile::emeraldOre->id || t == Tile::redStoneOre->id || t == Tile::redStoneOre_lit->id) return pick >= 3;
	if (t == Tile::obsidian->id) return pick >= 4;
	return true;
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
	if (t == Tile::gravel->id) return I_GRAVEL;
	if (t == Tile::stoneBrickSmooth->id) return I_STONEBRICK;
	if (t == Tile::glass->id) return I_GLASS;
	if (t == Tile::sand->id) return I_SAND;
	if (t == Tile::sapling->id) return I_SAPLING;
	if (t == Tile::chest->id) return I_PLANK; // (8 of them)
	return -1;
}

void PlayerNPC::collect(int t, int data)
{
	if (Tile::isLog(t)) inv[I_LOG]++;
	else if (t == Tile::rock->id || t == Tile::stoneBrick->id || t == Tile::mossStone->id) inv[I_COBBLE]++;
	else if (t == ((Tile*)Tile::grass)->id || t == Tile::dirt->id || t == Tile::farmland->id || t == Tile::podzol->id || t == Tile::mycelium->id) inv[I_DIRT]++;
	else if (t == Tile::wood->id) inv[I_PLANK]++;
	else if (t == Tile::gravel->id) inv[I_GRAVEL]++;
	else if (t == Tile::stoneBrickSmooth->id) inv[I_STONEBRICK]++;
	else if (t == Tile::coalOre->id) {
		inv[I_COAL]++;
		if (random.nextInt(4) == 0 && lastTopicSaid != T_COAL) say(SAY_LINES[T_COAL][random.nextInt(6)], T_COAL);
	} else if (t == Tile::ironOre->id) {
		if (!canHarvest(t)) return;
		inv[I_IRON]++;
		if (random.nextInt(3) == 0 && lastTopicSaid != T_IRON) say(SAY_LINES[T_IRON][random.nextInt(6)], T_IRON);
	} else if (t == Tile::goldOre->id) {
		if (!canHarvest(t)) return;
		inv[I_GOLD]++;
		if (random.nextInt(3) == 0 && lastTopicSaid != T_GOLD) say(SAY_LINES[T_GOLD][random.nextInt(6)], T_GOLD);
	} else if (t == Tile::emeraldOre->id) {
		if (!canHarvest(t)) return;
		inv[I_DIAMOND]++;
		if (lastTopicSaid != T_DIAMOND || random.nextInt(3) == 0) say(SAY_LINES[T_DIAMOND][random.nextInt(6)], T_DIAMOND);
	} else if (t == Tile::redStoneOre->id || t == Tile::redStoneOre_lit->id) { if (canHarvest(t)) inv[I_REDSTONE] += 4; }
	else if (t == Tile::lapisOre->id) { if (canHarvest(t)) inv[I_LAPIS] += 5; }
	else if (t == Tile::tallgrass->id) { if (random.nextInt(2) == 0) inv[I_SEEDS]++; }
	else if (t == Tile::crops->id) { if (data >= 7) { inv[I_WHEAT]++; inv[I_SEEDS] += 1 + random.nextInt(2); } else inv[I_SEEDS]++; }
	else if (t == Tile::sand->id) inv[I_SAND]++;
	else if (t == Tile::workBench->id) { if (inv[I_TABLE] > 0) inv[I_PLANK] += 4; else inv[I_TABLE]++; } // (one is enough)
	else if (t == Tile::furnace->id || t == Tile::furnace_lit->id) inv[I_FURNACE]++;
	else if (t == Tile::fence->id) inv[I_FENCE]++;
	else if (t == Tile::torch->id) inv[I_TORCH]++;
	else if (Tile::isLeaf(t)) { if (random.nextInt(20) == 0) inv[I_SAPLING]++; }
}

void PlayerNPC::setHeld(int t, bool block)
{
	if (block) {
		if (t > 0 && t < 256 && Tile::tiles[t]) held = ItemInstance(Tile::tiles[t]);
		else held = ItemInstance();
		if (t == Tile::door_wood->id) held = ItemInstance(Item::door_wood);
		if (t == Tile::crops->id) held = ItemInstance(Item::seeds_wheat);
		return;
	}
	// tool for breaking t
	Tile* tile = (t > 0 && t < 256) ? Tile::tiles[t] : NULL;
	if (tile && tile->material == Material::stone && pick)
		held = ItemInstance(pick == 4 ? Item::pickAxe_emerald : pick == 3 ? Item::pickAxe_iron : pick == 2 ? Item::pickAxe_stone : Item::pickAxe_wood);
	else if (tile && tile->material == Material::wood && axe)
		held = ItemInstance(axe >= 3 ? Item::hatchet_iron : axe == 2 ? Item::hatchet_stone : Item::hatchet_wood);
	else if (tile && (tile->material == Material::dirt || tile->material == Material::sand) && pick >= 2)
		held = ItemInstance(pick >= 3 ? Item::shovel_iron : Item::shovel_stone);
	else held = ItemInstance();
}

// would breaking this let lava or water in, or open a drop into a cave?
bool PlayerNPC::mineUnsafe(int bx, int by, int bz, bool& caveDrop)
{
	caveDrop = false;
	static const int d[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (int i = 0; i < 6; ++i)
		if (level->getMaterial(bx + d[i][0], by + d[i][1], bz + d[i][2])->isLiquid()) return true;
	// the floor of the next step: something has to be under it
	const int below = level->getTile(bx, by - 1, bz);
	if (by < Mth::floor(y - heightOffset) && below == 0 && level->getTile(bx, by - 2, bz) == 0) { caveDrop = true; return true; }
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
		if (!standSpotFor(bx, by, bz, standX, standY, standZ)) {
			// far away and no spot near it known yet: just head over there first
			const float fx = bx - x, fz = bz - z;
			if (fx * fx + fz * fz > 10 * 10) { walkTo((float)bx, (float)by, (float)bz, 3.0f); return 0; }
			if (log) printf("[npc] %s no stand spot for %d,%d,%d (at %.1f,%.1f,%.1f)\n", getNpcName().c_str(), bx, by, bz, x, y - heightOffset, z);
			standValid = false; return -1;
		}
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
		if (s.data == 7) {
			// a griefer's deed is done: the victim and whoever saw it will know
			NpcTown::get(level)->reportGrief(this, s.tile, s.x, s.y, s.z);
			if (random.nextInt(2) == 0) say(SAY_LINES[T_GRIEF][random.nextInt(6)], T_GRIEF);
		} else say(s.text);
		popStep();
		return;

	case Step::WAIT:
		if (s.data == 1 && swingTimer == 0) { swing(); swingTimer = 8; }
		if (s.data == 2) {
			// standing around: look about, jump, punch the air now and then
			if (random.nextInt(30) == 0) { yRot += random.nextFloat() * 120 - 60; xRot = random.nextFloat() * 40 - 20; }
			if (onGround && random.nextInt(90) == 0) jumping = true;
			if (swingTimer == 0 && random.nextInt(120) == 0) { swing(); swingTimer = 6; }
		}
		if (job == JOB_NIGHT && level->isDay()) { steps.clear(); return; }
		if (--s.ticks <= 0) popStep();
		return;

	case Step::CRAFT:
		if (s.data == 1) lookAtBlock(s.x, s.y, s.z);
		else if (s.ticks % 20 == 0) xRot = 50; // looking at its inventory
		if (s.data == 1 && swingTimer == 0 && s.ticks % 10 == 0) { swing(); swingTimer = 6; }
		held = ItemInstance();
		if (s.data == 1 && s.x != 0 && level->getTile(s.x, s.y, s.z) != Tile::workBench->id) { popStep(); return; } // the table's gone
		if (--s.ticks <= 0) { craftAll(s.data == 1); popStep(); }
		return;

	case Step::SMELT: {
		// at a furnace: light it, wait, take the iron out
		const int t = level->getTile(s.x, s.y, s.z);
		if (t != Tile::furnace->id && t != Tile::furnace_lit->id) { popStep(); return; }
		{
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) { popStep(); return; }
			if (a == 0) return;
		}
		lookAtBlock(s.x, s.y, s.z);
		if (t == Tile::furnace->id) {
			level->setTileAndData(s.x, s.y, s.z, Tile::furnace_lit->id, level->getData(s.x, s.y, s.z));
			swing();
		}
		held = ItemInstance();
		if (--s.ticks <= 0) {
			smeltAll();
			if (level->getTile(s.x, s.y, s.z) == Tile::furnace_lit->id) level->setTileAndData(s.x, s.y, s.z, Tile::furnace->id, level->getData(s.x, s.y, s.z));
			popStep();
		}
		return;
	}

	case Step::CHEST: {
		// the storage chests on the plaza: put things in or take things out
		{
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) { popStep(); return; }
			if (a == 0) return;
		}
		lookAtBlock(s.x, s.y, s.z);
		held = ItemInstance();
		if (s.ticks == 30) level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, "random.chestopen", 0.5f, level->random.nextFloat() * 0.1f + 0.9f);
		if (--s.ticks <= 0) {
			useChest(s);
			level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, "random.chestclosed", 0.5f, level->random.nextFloat() * 0.1f + 0.9f);
			popStep();
		}
		return;
	}

	case Step::MOVE: {
		if (s.data == 1) {
			// down the mine stairs: right above the step counts as not there yet, walk off the ledge
			const float hx = s.x + 0.5f - x, hz = s.z + 0.5f - z;
			const int feet = Mth::floor(y - heightOffset + 0.01f);
			if (hx * hx + hz * hz < 1.6f * 1.6f && feet > s.y && onGround) {
				yRot = (float)(Mth::atan2(hz, hx) * 180 / Mth::PI) - 90;
				yya = runSpeed * 0.5f;
				if (++s.ticks > 60) { mineFails++; steps.clear(); }
				return;
			}
		}
		const int r = walkTo((float)s.x, (float)s.y, (float)s.z, (float)Mth::Max(1, s.data == 1 ? 1 : s.ticks));
		if (r || routeFails > 5) {
			routeFails = 0;
			const Step done = s;
			popStep();
			if (done.data == 1 && !r) { mineFails++; steps.clear(); return; } // couldn't get down there: give up this time
			if (done.data == 3 && !r && job == JOB_CAVE) { markBad(done.x, done.y, done.z); return; }
			// in the mine: grab ore in the walls around
			if (r && done.data == 1) {
				mineDepth = Mth::Max(mineDepth, done.tile);
				mineFails = 0;
				const int fx = Mth::floor(x), fy = Mth::floor(y - heightOffset), fz = Mth::floor(z);
				for (int dy = 0; dy <= 1; ++dy) for (int k = 0; k < 4; ++k) {
					const int ox = fx + (k == 0) - (k == 1), oz = fz + (k == 2) - (k == 3);
					const int t = level->getTile(ox, fy + dy, oz);
					if (t == Tile::coalOre->id || t == Tile::ironOre->id || t == Tile::goldOre->id || t == Tile::emeraldOre->id || t == Tile::redStoneOre->id || t == Tile::lapisOre->id)
						pushFront(Step::BREAK, ox, fy + dy, oz, -1);
				}
				// lava or water ahead: stop this mine
				const int t = level->getTile(fx, fy - 1, fz);
				if (t == Tile::lava->id || t == Tile::calmLava->id) { steps.clear(); hasMine = false; }
				// dark down here: a torch on the wall
				if (inv[I_TORCH] > 0 && level->getRawBrightness(fx, fy, fz) < 8 && (done.tile % 5) == 0) pushFront(Step::PLACE, fx, fy, fz, Tile::torch->id, -2);
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
		if (job == JOB_MINE) {
			bool caveDrop;
			if (mineUnsafe(s.x, s.y, s.z, caveDrop)) {
				steps.clear(); hasMine = false;
				// broke into a cave: go and have a look
				if (caveDrop && (inv[I_TORCH] > 0 || role == ROLE_EXPLORER || role == ROLE_MINER)) {
					caveTrail.clear();
					Waypoint w = { Mth::floor(x), Mth::floor(y - heightOffset), Mth::floor(z) };
					caveTrail.push_back(w);
					startJob(JOB_CAVE);
					caveMoves = 0; returning = false;
					if (random.nextInt(2) == 0) say(SAY_LINES[T_CAVE][random.nextInt(6)], T_CAVE);
				}
				return;
			}
		}
		{
			const int a = approach(s.x, s.y, s.z);
			if (a < 0) {
				markBad(s.x, s.y, s.z); popStep();
				if (job == JOB_WOOD || job == JOB_MINE) { if (job == JOB_MINE) mineFails++; steps.clear(); }
				return;
			}
			if (a == 0) return;
		}
		route.destroy();
		lookAtBlock(s.x, s.y, s.z);
		setHeld(t, false);
		if (digProgress == 0) digNeeded = digTicks(t);
		if (digNeeded < 0) { popStep(); return; }
		if (swingTimer == 0) { swing(); swingTimer = 4; }
		// the crack spreading over the block, and bits flying off it
		if ((digProgress & 3) == 0) {
			const int stage = Mth::Min(9, digProgress * 10 / Mth::Max(1, digNeeded));
			level->levelEvent(NULL, LevelEvent::NPC_CRACK_BLOCK, s.x, s.y, s.z, 1 | ((stage + 1) << 8));
		}
		if ((digProgress % 5) == 0 && t > 0 && Tile::tiles[t]) {
			Tile* tile = Tile::tiles[t];
			level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, tile->soundType->getStepSound(), (tile->soundType->getVolume() + 1) / 8, tile->soundType->getPitch() * 0.5f);
		}
		if (++digProgress >= digNeeded) {
			const int data = level->getData(s.x, s.y, s.z);
			Tile* tile = Tile::tiles[t];
			level->levelEvent(NULL, LevelEvent::PARTICLES_DESTROY_BLOCK, s.x, s.y, s.z, t);
			if (tile) level->playSound(s.x + 0.5f, s.y + 0.5f, s.z + 0.5f, tile->soundType->getBreakSound(), (tile->soundType->getVolume() + 1) / 2, tile->soundType->getPitch() * 0.8f);
			if (t == Tile::door_wood->id) {
				// both halves
				const int oy = (data & 8) ? s.y - 1 : s.y + 1;
				if (level->getTile(s.x, oy, s.z) == t) level->setTile(s.x, oy, s.z, 0);
				inv[I_DOOR]++;
			}
			level->setTile(s.x, s.y, s.z, 0);
			if (t != Tile::door_wood->id) collect(t, data);
			// lumberjacks plant a sapling where the tree was
			if (job == JOB_WOOD && Tile::isLog(t) && role == ROLE_LUMBERJACK && inv[I_SAPLING] > 0) {
				const int below = level->getTile(s.x, s.y - 1, s.z);
				if (below == ((Tile*)Tile::grass)->id || below == Tile::dirt->id) { level->setTile(s.x, s.y, s.z, Tile::sapling->id); inv[I_SAPLING]--; }
			}
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
		held = ItemInstance(hoe >= 3 ? Item::hoe_iron : hoe == 2 ? Item::hoe_stone : Item::hoe_wood);
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
			Step go = s; go.kind = Step::MOVE; go.x = bx; go.y = by; go.z = bz; go.tile = 0; go.data = 0; go.ticks = 3;
			Step fill = s; fill.kind = Step::WAIT; fill.x = fill.y = fill.z = 0; fill.tile = 0; fill.data = 1; fill.ticks = 15;
			steps.push_front(fill);
			steps.push_front(go);
			hasWater = true; // after the walk and the swing
			return;
		}
		// filling holes: any block will do
		if (s.data == -1 && itemFor(s.tile) >= 0 && inv[itemFor(s.tile)] <= 0) {
			if (inv[I_DIRT] > 0) s.tile = Tile::dirt->id;
			else if (inv[I_COBBLE] > 0) s.tile = Tile::stoneBrick->id;
			else if (inv[I_GRAVEL] > 0 && level->getMaterial(s.x, s.y - 1, s.z)->blocksMotion()) s.tile = Tile::gravel->id;
			else if (inv[I_PLANK] > 8) s.tile = Tile::wood->id;
			else if (s.proj < 0) { popStep(); return; }
		}
		// stone bricks and glass: plain cobble / no window will do if it has none
		if (s.tile == Tile::stoneBrickSmooth->id && inv[I_STONEBRICK] <= 0 && inv[I_COBBLE] > 0) { s.tile = Tile::stoneBrick->id; data = 0; }
		if (s.tile == Tile::glass->id && inv[I_GLASS] <= 0) { if (optional || s.proj < 0) { popStep(); return; } s.tile = Tile::fence->id; }
		if (s.tile == Tile::gravel->id && inv[I_GRAVEL] <= 0 && inv[I_COBBLE] > 0) s.tile = Tile::stoneBrick->id;
		const int item = itemFor(s.tile);
		const int cost = s.tile == Tile::chest->id ? 8 : 1;
		if (item >= 0 && inv[item] < cost) {
			if (optional) { popStep(); return; }
			// nothing it goes out to gather: do without
			if (item != I_PLANK && item != I_COBBLE && item != I_LOG && item != I_FENCE && item != I_GATE && item != I_DIRT && item != I_TORCH && item != I_STONEBRICK && item != I_GRAVEL) { popStep(); return; }
			// out of material: stop here and go get more (the plan remembers where it was)
			if ((job == JOB_BUILD || job == JOB_EXTRA) && s.ticks < (int)planPos) planPos = s.ticks;
			if (s.proj >= 0) {
				NpcTown* town = NpcTown::get(level);
				if (s.proj < (int)town->projects.size() && s.ticks < (int)town->projects[s.proj].pos) town->projects[s.proj].pos = s.ticks;
			}
			steps.clear();
			job = JOB_NONE;
			return;
		}
		if (s.tile == Tile::crops->id && level->getTile(s.x, s.y - 1, s.z) != Tile::farmland->id) { popStep(); return; }
		if (s.tile == Tile::sapling->id && level->getTile(s.x, s.y - 1, s.z) != ((Tile*)Tile::grass)->id && level->getTile(s.x, s.y - 1, s.z) != Tile::dirt->id) { popStep(); return; }
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
		// placing below its feet or up high: crouch at the edge like a player
		const int feet = Mth::floor(y - heightOffset);
		if (s.y < feet || (s.y >= feet + 1 && !level->getMaterial(Mth::floor(x), feet - 2, Mth::floor(z))->blocksMotion())) sneakNow = true;
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
		if (item >= 0) inv[item] -= cost;
		if (s.tile == Tile::calmWater->id) { hasWater = false; waterPlaced = true; }
		if (s.tile == Tile::torch->id && job == JOB_CAVE) { caveTorchX = s.x; caveTorchY = s.y; caveTorchZ = s.z; }
		popStep();
		return;
	}

	case Step::PILLAR: {
		const int fy = Mth::floor(y - heightOffset);
		if (++s.ticks > 60 || std::abs(Mth::floor(x) - s.x) + std::abs(Mth::floor(z) - s.z) > 0) { popStep(); return; }
		if (level->getMaterial(s.x, s.y + 2, s.z)->blocksMotion()) { popStep(); return; } // the head block gets dug first
		jumping = true;
		xRot = 80;
		if (fy >= s.y + 1 && (level->getTile(s.x, s.y, s.z) == 0 || level->getMaterial(s.x, s.y, s.z)->isLiquid())) {
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

	case Step::PICKUP: // (kept for old saves) same as breaking it
		s.kind = Step::BREAK;
		return;

	case Step::LEAD: {
		Entity* e = level->getEntity(s.data);
		if (!e || !e->isAlive() || --s.ticks <= 0) { popStep(); if (job == JOB_FIGHT) fightTarget = 0; return; }
		if (job == JOB_FIGHT) {
			// hit it until it dies or runs
			if (e->distanceToSqr(this) > 24 * 24) { popStep(); fightTarget = 0; return; }
			held = ItemInstance(sword == 3 ? Item::sword_iron : sword == 2 ? Item::sword_stone : Item::sword_wood);
			if (sword == 0) held = ItemInstance();
			if (e->distanceToSqr(this) > 2.2f * 2.2f) { walkTo(e->x - 0.5f, e->y - e->heightOffset, e->z - 0.5f, 1.5f); return; }
			lookAt(e, 30, 30);
			// players strafe and jump around in a fight
			xxa = (s.ticks / 15) % 2 ? 0.4f : -0.4f;
			if (onGround && random.nextInt(12) == 0) jumping = true;
			if (swingTimer == 0) { swing(); swingTimer = 10; e->hurt(this, sword == 3 ? 6 : sword == 2 ? 5 : sword == 1 ? 4 : 1); }
			return;
		}
		if (job == JOB_FLEE) {
			// run the other way
			const float fx = x - e->x, fz = z - e->z;
			const float d = Mth::sqrt(fx * fx + fz * fz) + 0.01f;
			if (d > 20) { popStep(); return; }
			yRot = (float)(Mth::atan2(fz / d, fx / d) * 180 / Mth::PI) - 90;
			yya = runSpeed;
			sprinting = true;
			if (horizontalCollision) jumping = true;
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
