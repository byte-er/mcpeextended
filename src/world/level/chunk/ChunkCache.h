#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

//package net.minecraft.world.level.chunk;

#include "ChunkSource.h"
#include <vector>
#include "../storage/LevelData.h"
#include "storage/ChunkStorage.h"
#include "EmptyLevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"

class ChunkCache: public ChunkSource {
    //static const int CHUNK_CACHE_WIDTH = CHUNK_CACHE_WIDTH; // WAS 32;
    static const int MAX_SAVES = 2;
public:
    ChunkCache(Level* level_, ChunkStorage* storage_, ChunkSource* source_)
	:	xLast(-999999999),
		zLast(-999999999),
		last(NULL),
		level(level_),
		storage(storage_),
		source(source_)
	{
		isChunkCache = true;
        //emptyChunk = new EmptyLevelChunk(level_, emptyChunkBlocks, 0, 0);
		emptyChunk = new EmptyLevelChunk(level_, NULL, 0, 0);
		memset(chunks, 0, sizeof(LevelChunk*) * CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH);
    }

	~ChunkCache() {
		savePending(-1);
		delete source;
		delete emptyChunk;

		for (int i = 0; i < CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH; i++)
		{
			if (chunks[i])
			{
				chunks[i]->deleteBlockData();
				delete chunks[i];
			}
		}
	}

    bool fits(int x, int z) {
        return (x >= 0 && z >= 0 && x < CHUNK_CACHE_WIDTH && z < CHUNK_CACHE_WIDTH);
    }

    bool hasChunk(int x, int z) {
		// with a fixed world size, chunks outside the fitting area are always available (emptyChunks)
        if (!fits(x, z)) return true;

        if (x == xLast && z == zLast && last != NULL) {
            return true;
        }
        int xs = x & (CHUNK_CACHE_WIDTH - 1);
        int zs = z & (CHUNK_CACHE_WIDTH - 1);
        int slot = xs + zs * CHUNK_CACHE_WIDTH;
        return chunks[slot] != NULL && (chunks[slot] == emptyChunk || chunks[slot]->isAt(x, z));
    }

    LevelChunk* create(int x, int z) {
        return getChunk(x, z);
    }

    LevelChunk* getChunk(int x, int z) {
		//static Stopwatch sw;
		//sw.start();

		if (x == xLast && z == zLast && last != NULL) {
            return last;
        }
		if (!fits(x, z)) return emptyChunk;
        //if (!level->isFindingSpawn && !fits(x, z)) return emptyChunk;
        int xs = x & (CHUNK_CACHE_WIDTH - 1);
        int zs = z & (CHUNK_CACHE_WIDTH - 1);
        int slot = xs + zs * CHUNK_CACHE_WIDTH;
        if (!hasChunk(x, z)) {
            if (chunks[slot] != NULL) {
                chunks[slot]->unload();
                save(chunks[slot]);
                saveEntities(chunks[slot]);
            }

            LevelChunk* newChunk = load(x, z);
			bool updateLights = false;
            if (newChunk == NULL) {
                if (source == NULL) {
                    newChunk = emptyChunk;
                } else {
                    newChunk = source->getChunk(x, z);
                }
            } else {
				//return emptyChunk;
				// old saves have no light data; infinite worlds always save it
				updateLights = !level->isInfinite();
            }
            chunks[slot] = newChunk;
            newChunk->lightLava();

			if (updateLights)
			{
				for (int cx = 0; cx < 16; cx++)
				{
					for (int cz = 0; cz < 16; cz++)
					{
						int height = level->getHeightmap(cx + x * 16, cz + z * 16);
						for (int cy = height; cy >= 0; cy--)
						{
							level->updateLight(LightLayer::Sky, cx + x * 16, cy, cz + z * 16, cx + x * 16, cy, cz + z * 16);
							level->updateLight(LightLayer::Block, cx + x * 16 - 1, cy, cz + z * 16 - 1, cx + x * 16 + 1, cy, cz + z * 16 + 1);
						}
					}
				}
				//level->updateLight(LightLayer::Sky, x * 16, 0, z * 16, x * 16 + 15, 128, z * 16 + 15);
				//level->updateLight(LightLayer::Block, x * 16, 0, z * 16, x * 16 + 15, 128, z * 16 + 15);
			}

            if (chunks[slot] != NULL) {
                chunks[slot]->load();
            }

            if (!chunks[slot]->terrainPopulated && hasChunk(x + 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x + 1, z)) postProcess(this, x, z);
            if (hasChunk(x - 1, z) && !getChunk(x - 1, z)->terrainPopulated && hasChunk(x - 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z);
            if (hasChunk(x, z - 1) && !getChunk(x, z - 1)->terrainPopulated && hasChunk(x + 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x + 1, z)) postProcess(this, x, z - 1);
            if (hasChunk(x - 1, z - 1) && !getChunk(x - 1, z - 1)->terrainPopulated && hasChunk(x - 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z - 1);
        }
        xLast = x;
        zLast = z;
        last = chunks[slot];

		//sw.stop();
		//sw.printEvery(500000, "ChunkCache::load: ");

        return chunks[slot];
    }

	Biome::MobList getMobsAt(const MobCategory& mobCategory, int x, int y, int z) {
		return source->getMobsAt(mobCategory, x, y, z);
	}

    void postProcess(ChunkSource* parent, int x, int z) {
		if (!fits(x, z)) return;
        LevelChunk* chunk = getChunk(x, z);
        if (!chunk->terrainPopulated) {
            chunk->terrainPopulated = true;
            if (source != NULL) {
                source->postProcess(parent, x, z);
				chunk->clearUpdateMap();
            }
        }
    }

    //bool save(bool force, ProgressListener progressListener) {
    //    int saves = 0;
    //    int count = 0;
    //    if (progressListener != NULL) {
    //        for (int i = 0; i < chunks.length; i++) {
    //            if (chunks[i] != NULL && chunks[i].shouldSave(force)) {
    //                count++;
    //            }
    //        }
    //    }
    //    int cc = 0;
    //    for (int i = 0; i < chunks.length; i++) {
    //        if (chunks[i] != NULL) {
    //            if (force && !chunks[i].dontSave) saveEntities(chunks[i]);
    //            if (chunks[i].shouldSave(force)) {
    //                save(chunks[i]);
    //                chunks[i].unsaved = false;
    //                if (++saves == MAX_SAVES && !force) return false;
    //                if (progressListener != NULL) {
    //                    if (++cc % 10 == 0) {
    //                        progressListener.progressStagePercentage(cc * 100 / count);
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    if (force) {
    //        if (storage == NULL) return true;
    //        storage.flush();
    //    }
    //    return true;
    //}

    bool tick() {
        if (storage != NULL) storage->tick();
        return source->tick();
    }

    bool shouldSave() {
        return true;
    }

    std::string gatherStats() {
        return "ChunkCache: 1024";
    }
	
	/**
	 * Infinite worlds: the window moved by (dcx, dcz) chunks. Chunks still in
	 * the window move to their new slot (their local coordinates change);
	 * the others are saved and freed. Missing ones load lazily as before.
	 */
	void shiftWindow(int dcx, int dcz) {
		LevelChunk* moved[CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH];
		memset(moved, 0, sizeof(moved));
		for (int i = 0; i < CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH; ++i) {
			LevelChunk* c = chunks[i];
			if (!c || c == emptyChunk) continue;
			const int nx = c->x - dcx, nz = c->z - dcz;
			if (fits(nx, nz)) {
				c->x = nx;
				c->z = nz;
				moved[nx + nz * CHUNK_CACHE_WIDTH] = c;
			} else {
				// saved a few per tick (savePending), not all at once during the shift
				c->unload();
				PendingSave p;
				p.wx = c->x + level->getLevelData()->originX;
				p.wz = c->z + level->getLevelData()->originZ;
				p.chunk = c;
				pendingSaves.push_back(p);
			}
		}
		memcpy(chunks, moved, sizeof(chunks));
		last = NULL;
		xLast = zLast = -999999999;
		if (source) source->windowShifted();
	}

	/** Infinite worlds: is local chunk (x, z) in memory (no generation / loading)? */
	bool isLoaded(int x, int z) {
		if (!fits(x, z)) return false;
		LevelChunk* c = chunks[(x & (CHUNK_CACHE_WIDTH - 1)) + (z & (CHUNK_CACHE_WIDTH - 1)) * CHUNK_CACHE_WIDTH];
		return c != NULL && c != emptyChunk && c->isAt(x, z);
	}

	/** Infinite worlds: write up to maxCount chunks that left the window (-1: all). */
	void savePending(int maxCount) {
		int n = 0;
		while (!pendingSaves.empty() && (maxCount < 0 || n < maxCount)) {
			PendingSave p = pendingSaves.front();
			pendingSaves.erase(pendingSaves.begin());
			LevelChunk* c = p.chunk;
			// local coordinates relative to the current window: the storage adds the origin
			c->x = p.wx - level->getLevelData()->originX;
			c->z = p.wz - level->getLevelData()->originZ;
			save(c);
			c->deleteBlockData();
			delete c;
			++n;
		}
	}

	void saveAll(bool onlyUnsaved) {
		savePending(-1);
		if (storage != NULL) {
			std::vector<LevelChunk*> chunks;
			for (int z = 0; z < CHUNK_CACHE_WIDTH; ++z)
			for (int x = 0; x < CHUNK_CACHE_WIDTH; ++x) {
				if (level->isInfinite() && !isLoaded(x, z)) continue; // nothing to save, don't generate
				LevelChunk* chunk = level->getChunk(x, z);
				if (!onlyUnsaved || chunk->shouldSave(false))
					chunks.push_back( chunk );
			}
			storage->saveAll(level, chunks);
		}
	}
private:
	struct PendingSave { int wx, wz; LevelChunk* chunk; };
	std::vector<PendingSave> pendingSaves;

    LevelChunk* load(int x, int z) {
		// walked back before it was written: take it from the save queue
		if (!pendingSaves.empty()) {
			const int wx = x + level->getLevelData()->originX, wz = z + level->getLevelData()->originZ;
			for (unsigned int i = 0; i < pendingSaves.size(); ++i) {
				if (pendingSaves[i].wx == wx && pendingSaves[i].wz == wz) {
					LevelChunk* c = pendingSaves[i].chunk;
					pendingSaves.erase(pendingSaves.begin() + i);
					c->x = x;
					c->z = z;
					return c;
				}
			}
		}
        if (storage == NULL) return emptyChunk;
		if (x < 0 || x >= CHUNK_CACHE_WIDTH || z < 0 || z >= CHUNK_CACHE_WIDTH)
		{
			return emptyChunk;
		}
        //try {
            LevelChunk* levelChunk = storage->load(level, x, z);
            if (levelChunk != NULL) {
                levelChunk->lastSaveTime = level->getTime();
            }
            return levelChunk;
        //} catch (Exception e) {
        //    e.printStackTrace();
        //    return emptyChunk;
        //}
    }

    void saveEntities(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            storage->saveEntities(level, levelChunk);
        //} catch (Error e) {
        //    e.printStackTrace();
        //}
    }

    void save(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            levelChunk->lastSaveTime = level->getTime();
            storage->save(level, levelChunk);
        //} catch (IOException e) {
        //    e.printStackTrace();
        //}
    }

public:
	int xLast;
    int zLast;
private:
	//unsigned char emptyChunkBlocks[LevelChunk::ChunkBlockCount];
    LevelChunk* emptyChunk;
    ChunkSource* source;
    ChunkStorage* storage;
    LevelChunk* chunks[CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH];
    Level* level;

    LevelChunk* last;

};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/
