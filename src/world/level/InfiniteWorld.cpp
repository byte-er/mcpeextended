// Infinite worlds (LevelSettings::WORLD_INFINITE).
//
// The engine keeps a fixed 512x512 level and uses float positions, which stop
// working millions of blocks from the origin. So an infinite world is a
// 512x512 window over a 60,000,000 block world: LevelData::originX/Z is the
// world chunk of local chunk (0, 0). Terrain, biomes and the region files on
// disk use world coordinates; everything inside the level uses window
// coordinates. When the player walks away from the middle, shiftWindow()
// moves the window: chunks, entities, tile entities, scheduled ticks and light
// updates keep their world position (their local coordinates change), and
// whatever leaves the window is saved and brought back when it is in range
// again.

#include "Level.h"
#include "LevelConstants.h"
#include "LevelSettings.h"
#include "LightUpdate.h"
#include "TickNextTickData.h"
#include "BiomeTint.h"
#include "LevelListener.h"
#include "chunk/ChunkCache.h"
#include "chunk/LevelChunk.h"
#include "storage/LevelStorage.h"
#include "tile/entity/TileEntity.h"
#include "../entity/Entity.h"
#include "../entity/EntityFactory.h"
#include "../entity/Mob.h"
#include "../entity/HangingEntity.h"
#include "../entity/player/Player.h"
#include "../../nbt/CompoundTag.h"
#include "../../nbt/ListTag.h"
#include "../../nbt/FloatTag.h"
#include "../../util/Mth.h"
#include <cmath>

bool Level::isInfinite() {
	return levelData.worldType == LevelSettings::WORLD_INFINITE;
}

static bool inWindow(float x, float z) {
	return x >= 0 && z >= 0 && x < LEVEL_WIDTH && z < LEVEL_DEPTH;
}

// Moves an entity (and everything that remembers where it was) by (dx, dz).
static void moveEntity(Entity* e, float dx, float dz, int dcx, int dcz) {
	e->x += dx;    e->z += dz;
	e->xo += dx;   e->zo += dz;
	e->xOld += dx; e->zOld += dz;
	e->bb.x0 += dx; e->bb.x1 += dx;
	e->bb.z0 += dz; e->bb.z1 += dz;
	e->xChunk += dcx;
	e->zChunk += dcz;
	if (e->isMob()) {
		((Mob*)e)->shiftLerpTarget(dx, dz);
	}
	if (e->isPlayer()) {
		Player* p = (Player*)e;
		p->bedPosition.x += (int)dx; p->bedPosition.z += (int)dz;
		Pos r = p->getRespawnPosition();
		r.x += (int)dx; r.z += (int)dz;
		p->setRespawnPosition(r);
	}
}

// Positions inside a saved tag: entities have a "Pos" float list, tile entities x/y/z ints
static void moveTag(CompoundTag* tag, bool tile, float dx, float dz) {
	if (tile) {
		tag->putInt("x", tag->getInt("x") + (int)dx);
		tag->putInt("z", tag->getInt("z") + (int)dz);
		return;
	}
	if (!tag->contains("Pos", Tag::TAG_List)) return;
	ListTag* pos = tag->getList("Pos");
	if (pos->size() < 3) return;
	Tag* tx = pos->get(0);
	Tag* tz = pos->get(2);
	if (tx && tx->getId() == Tag::TAG_Float) ((FloatTag*)tx)->data += dx;
	if (tz && tz->getId() == Tag::TAG_Float) ((FloatTag*)tz)->data += dz;
	if (tag->contains("TileX")) tag->putInt("TileX", tag->getInt("TileX") + (int)dx); // paintings
	if (tag->contains("TileZ")) tag->putInt("TileZ", tag->getInt("TileZ") + (int)dz);
}

static void tagPos(CompoundTag* tag, bool tile, float& x, float& z) {
	x = z = -1e9f;
	if (tile) { x = (float)tag->getInt("x"); z = (float)tag->getInt("z"); return; }
	if (!tag->contains("Pos", Tag::TAG_List)) return;
	ListTag* pos = tag->getList("Pos");
	if (pos->size() < 3) return;
	Tag* tx = pos->get(0);
	Tag* tz = pos->get(2);
	if (tx && tx->getId() == Tag::TAG_Float) x = ((FloatTag*)tx)->data;
	if (tz && tz->getId() == Tag::TAG_Float) z = ((FloatTag*)tz)->data;
}

void Level::shiftWindow(int dcx, int dcz) {
	if (!isInfinite() || (dcx == 0 && dcz == 0) || !_chunkSource || !_chunkSource->isChunkCache) return;

	// everything waiting to be written goes to the old window's chunk files first
	if (levelStorage) levelStorage->flushPendingChunks();

	const float dx = -16.0f * dcx, dz = -16.0f * dcz;

	// entities: leaving ones are saved and removed (the tick removes them for good)
	for (unsigned int i = 0; i < entities.size(); ++i) {
		Entity* e = entities[i];
		if (e->removed) continue;
		if (e->isPlayer() || inWindow(e->x + dx, e->z + dz)) {
			moveEntity(e, dx, dz, -dcx, -dcz);
			if (e->isPlayer()) continue;
			if (e->isHangingEntity()) {
				HangingEntity* h = (HangingEntity*)e;
				h->xTile += (int)dx;
				h->zTile += (int)dz;
			}
			continue;
		}
		CompoundTag* tag = new CompoundTag();
		if (e->save(tag)) {
			FarEntry fe = { levelData.originX, levelData.originZ, false, tag };
			farEntities.push_back(fe);
		} else {
			delete tag;
		}
		if (e->inChunk && hasChunk(e->xChunk, e->zChunk)) getChunk(e->xChunk, e->zChunk)->removeEntity(e);
		e->inChunk = false;
		e->remove();
	}

	// tile entities: their chunk leaves with them, so save and free the leaving ones now
	for (unsigned int i = 0; i < tileEntities.size(); ++i) {
		TileEntity* te = tileEntities[i];
		if (inWindow(te->x + dx, te->z + dz)) {
			te->x += (int)dx;
			te->z += (int)dz;
			continue;
		}
		if (te->shouldSave()) {
			CompoundTag* tag = new CompoundTag();
			if (te->save(tag)) {
				FarEntry fe = { levelData.originX, levelData.originZ, true, tag };
				farEntities.push_back(fe);
			} else {
				delete tag;
			}
		}
		Util::remove(pendingTileEntities, te);
		tileEntities.erase(tileEntities.begin() + (i--));
		delete te;
	}
	for (unsigned int i = 0; i < pendingTileEntities.size(); ++i) {
		TileEntity* te = pendingTileEntities[i];
		te->x += (int)dx;
		te->z += (int)dz;
	}

	// chunks
	((ChunkCache*)_chunkSource)->shiftWindow(dcx, dcz);

	// scheduled tile ticks and light updates
	{
		TickDataSet moved;
		for (TickDataSet::iterator it = _tickNextTickSet.begin(); it != _tickNextTickSet.end(); ++it) {
			TickNextTickData t = *it;
			t.x += (int)dx;
			t.z += (int)dz;
			if (inWindow((float)t.x, (float)t.z)) moved.insert(t);
		}
		_tickNextTickSet.swap(moved);
	}
	{
		LightUpdateList moved;
		for (unsigned int i = 0; i < _lightUpdates.size(); ++i) {
			LightUpdate u = _lightUpdates[i];
			u.x0 += (int)dx; u.x1 += (int)dx;
			u.z0 += (int)dz; u.z1 += (int)dz;
			if (u.x1 >= 0 && u.z1 >= 0 && u.x0 < LEVEL_WIDTH && u.z0 < LEVEL_DEPTH) moved.push_back(u);
		}
		_lightUpdates.swap(moved);
	}
	_chunksToPoll.clear();

	// level data: origin, spawn and the world border keep their world position
	levelData.originX += dcx;
	levelData.originZ += dcz;
	levelData.setSpawn(levelData.getXSpawn() + (int)dx, levelData.getYSpawn(), levelData.getZSpawn() + (int)dz);
	WorldBorder& border = getWorldBorder();
	border.setCenter(border.getCenterX() + dx, border.getCenterZ() + dz);

	restoreFarEntities();

	// colours are cached by local column; renderers rebuild from the moved chunks
	BiomeTint::clearCache();
	for (unsigned int i = 0; i < _listeners.size(); ++i) _listeners[i]->windowShifted((int)dx, (int)dz);

	// keep level.dat (origin + player) and entities.dat in step with the moved window
	if (levelStorage) {
		levelStorage->saveLevelData(levelData, &players);
		levelStorage->saveGame(this);
	}
}

void Level::restoreFarEntities() {
	for (unsigned int i = 0; i < farEntities.size(); ++i) {
		FarEntry& fe = farEntities[i];
		const float dx = 16.0f * (fe.ox - levelData.originX), dz = 16.0f * (fe.oz - levelData.originZ);
		float x, z;
		tagPos(fe.tag, fe.tile, x, z);
		if (!inWindow(x + dx, z + dz)) continue;
		moveTag(fe.tag, fe.tile, dx, dz);
		if (fe.tile) {
			if (TileEntity* te = TileEntity::loadStatic(fe.tag)) {
				LevelChunk* chunk = getChunkAt(te->x, te->z);
				if (chunk && !chunk->hasTileEntityAt(te)) chunk->addTileEntity(te);
				else delete te;
			}
		} else if (Entity* e = EntityFactory::loadEntity(fe.tag, this)) {
			addEntity(e);
		}
		fe.tag->deleteChildren();
		delete fe.tag;
		farEntities.erase(farEntities.begin() + (i--));
	}
}

bool Level::recentreWindow(float x, float z, float margin) {
	if (!isInfinite()) return false;
	const float mid = LEVEL_WIDTH * 0.5f;
	int dcx = 0, dcz = 0;
	if (std::fabs(x - mid) > margin) dcx = Mth::floor((x - mid) / 16.0f);
	if (std::fabs(z - mid) > margin) dcz = Mth::floor((z - mid) / 16.0f);
	if (!dcx && !dcz) return false;
	shiftWindow(dcx, dcz);
	return true;
}
