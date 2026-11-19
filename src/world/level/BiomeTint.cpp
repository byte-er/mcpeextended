#include "BiomeTint.h"
#include "Level.h"
#include "biome/BiomeSource.h"
#include "biome/Biome.h"
#include <map>
#include <cstring>

namespace {
	int g_grassMap[256 * 256];
	int g_foliageMap[256 * 256];
	bool g_haveMaps = false;

	struct ColumnBlock {
		int grass[256];
		int foliage[256];
	};

	BiomeSource* g_source = NULL;
	long g_seed = 0;
	std::map<long long, ColumnBlock*> g_cache;

	std::map<long long, ColumnBlock*> g_raw;
	void clearCache() {
		for (std::map<long long, ColumnBlock*>::iterator it = g_cache.begin(); it != g_cache.end(); ++it)
			delete it->second;
		g_cache.clear();
		for (std::map<long long, ColumnBlock*>::iterator it = g_raw.begin(); it != g_raw.end(); ++it)
			delete it->second;
		g_raw.clear();
	}

	// Java GrassColor/FoliageColor.get(temperature, rainfall)
	inline int lookup(const int* map, float temp, float rain) {
		rain *= temp;
		int x = (int)((1.0f - temp) * 255.0f);
		int y = (int)((1.0f - rain) * 255.0f);
		if (x < 0) x = 0; if (x > 255) x = 255;
		if (y < 0) y = 0; if (y > 255) y = 255;
		return map[(y << 8) | x];
	}

	// Per-biome climate as in PE 0.9.0 / Java 1.7 (temperature, rainfall),
	// clamped to 0..1 for the colormap lookup. Swamps use Java's fixed colour.
	void biomeColors(Biome* b, int& grass, int& foliage) {
		float t = 0.8f, r = 0.4f;                                   // plains
		if (b == Biome::swampland) { grass = 0x6a7039; foliage = 0x6a7039; return; }
		if (b == Biome::rainForest)                                 { t = 0.95f; r = 0.9f; }  // jungle
		else if (b == Biome::forest || b == Biome::seasonalForest)  { t = 0.7f;  r = 0.8f; }
		else if (b == Biome::taiga)                                 { t = 0.25f; r = 0.8f; }
		else if (b == Biome::savanna || b == Biome::desert)         { t = 1.0f;  r = 0.0f; }
		else if (b == Biome::iceDesert || b == Biome::tundra)       { t = 0.0f;  r = 0.5f; }
		else if (b == Biome::shrubland)                             { t = 0.85f; r = 0.3f; }
		grass   = lookup(g_grassMap, t, r);
		foliage = lookup(g_foliageMap, t, r);
	}


	inline long long keyOf(int bx, int bz) { return ((long long)bx << 32) ^ (unsigned int)bz; }

	const ColumnBlock* rawBlock(int bx, int bz) {
		const long long key = keyOf(bx, bz);
		std::map<long long, ColumnBlock*>::iterator it = g_raw.find(key);
		if (it != g_raw.end()) return it->second;
		ColumnBlock* b = new ColumnBlock();
		Biome** biomes = g_source->getBiomeBlock(bx << 4, bz << 4, 16, 16);
		for (int i = 0; i < 256; ++i) biomeColors(biomes[i], b->grass[i], b->foliage[i]);
		g_raw[key] = b;
		return b;
	}

	const int BLEND = 3; // 7x7 columns, like the smooth biome blend in 0.9.0

	const ColumnBlock* block(int x, int z) {
		const int bx = x >> 4, bz = z >> 4;
		const long long key = keyOf(bx, bz);
		std::map<long long, ColumnBlock*>::iterator it = g_cache.find(key);
		if (it != g_cache.end()) return it->second;

		// gather a 3x3 neighbourhood of raw blocks, box-blur the colours
		const ColumnBlock* nb[3][3];
		for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) nb[i][j] = rawBlock(bx + i - 1, bz + j - 1);
		ColumnBlock* b = new ColumnBlock();
		for (int lx = 0; lx < 16; ++lx)
		for (int lz = 0; lz < 16; ++lz) {
			int gr = 0, gg = 0, gb = 0, fr = 0, fg = 0, fb = 0, n = 0;
			for (int dx = -BLEND; dx <= BLEND; ++dx)
			for (int dz = -BLEND; dz <= BLEND; ++dz) {
				const int px = lx + dx + 16, pz = lz + dz + 16;      // 0..47
				const ColumnBlock* c = nb[px >> 4][pz >> 4];
				const int idx = ((px & 15) << 4) | (pz & 15);
				const int g = c->grass[idx], f = c->foliage[idx];
				gr += (g >> 16) & 255; gg += (g >> 8) & 255; gb += g & 255;
				fr += (f >> 16) & 255; fg += (f >> 8) & 255; fb += f & 255;
				++n;
			}
			const int idx = (lx << 4) | lz;
			b->grass[idx]   = ((gr / n) << 16) | ((gg / n) << 8) | (gb / n);
			b->foliage[idx] = ((fr / n) << 16) | ((fg / n) << 8) | (fb / n);
		}
		if (g_cache.size() > 8192) clearCache(); // far beyond any world we render
		g_cache[key] = b;
		return b;
	}
}

void BiomeTint::setColormaps(const unsigned char* grass, const unsigned char* foliage) {
	if (!grass || !foliage) return;
	for (int i = 0; i < 256 * 256; ++i) {
		g_grassMap[i]   = (grass[i * 4] << 16)   | (grass[i * 4 + 1] << 8)   | grass[i * 4 + 2];
		g_foliageMap[i] = (foliage[i * 4] << 16) | (foliage[i * 4 + 1] << 8) | foliage[i * 4 + 2];
	}
	g_haveMaps = true;
	clearCache();
}

void BiomeTint::setLevel(Level* level) {
	if (level && g_source && g_seed == level->getSeed()) return;
	clearCache();
	delete g_source;
	g_source = NULL;
	if (level) {
		g_seed = level->getSeed();
		g_source = new BiomeSource(level);
	}
}

int BiomeTint::grass(int x, int z) {
	if (!g_haveMaps || !g_source) return DEFAULT_GRASS;
	return block(x, z)->grass[((x & 15) << 4) | (z & 15)];
}

int BiomeTint::foliage(int x, int z) {
	if (!g_haveMaps || !g_source) return DEFAULT_FOLIAGE;
	return block(x, z)->foliage[((x & 15) << 4) | (z & 15)];
}
