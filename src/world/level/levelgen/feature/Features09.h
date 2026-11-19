#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN_FEATURE__Features09_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN_FEATURE__Features09_H__

// World generation features modelled on Minecraft PE 0.9.0 / Java 1.7:
// acacia + dark oak trees, flowers with data, ice spikes, badlands surface,
// podzol and mycelium patches.

#include <cmath>
#include <cstdlib>
#include "Feature.h"
#include "../../../../util/Random.h"
#include "../../Level.h"
#include "../../tile/Tile.h"
#include "../../tile/Bush.h"
#include "../../tile/Tiles09.h"

namespace Gen09 {
	inline bool isSoil(int t) {
		return t == ((Tile*)Tile::grass)->id || t == Tile::dirt->id || t == Tile::podzol->id;
	}
	inline bool canReplace(int t) {
		return t == 0 || Tile::isLeaf(t) || t == Tile::tallgrass->id || t == Tile::topSnow->id;
	}
}

/** Flower patch that places a tile together with a data value. */
class DataFlowerFeature : public Feature {
public:
	DataFlowerFeature(int tile, int data) : tile(tile), data(data) {}

	bool place(Level* level, Random* random, int x, int y, int z) {
		for (int i = 0; i < 64; i++) {
			int x2 = x + random->nextInt(8) - random->nextInt(8);
			int y2 = y + random->nextInt(4) - random->nextInt(4);
			int z2 = z + random->nextInt(8) - random->nextInt(8);
			if (level->isEmptyTile(x2, y2, z2) && ((Bush*)Tile::tiles[tile])->canSurvive(level, x2, y2, z2))
				level->setTileAndDataNoUpdate(x2, y2, z2, tile, data);
		}
		return true;
	}
private:
	int tile, data;
};

/** Savanna acacia: a trunk that leans to one side, ending in flat leaf canopies. */
class AcaciaFeature : public Feature {
public:
	AcaciaFeature(bool doUpdate = false) : Feature(doUpdate) {}

	bool place(Level* level, Random* random, int x, int y, int z) {
		const int height = random->nextInt(3) + random->nextInt(3) + 5;
		if (y < 1 || y + height + 1 > Level::DEPTH) return false;
		if (!Gen09::isSoil(level->getTile(x, y - 1, z))) return false;
		for (int yy = y; yy <= y + height + 1; ++yy)
			for (int xx = x - 2; xx <= x + 2; ++xx)
				for (int zz = z - 2; zz <= z + 2; ++zz)
					if (yy - y > 1 && !Gen09::canReplace(level->getTile(xx, yy, zz))) return false;

		placeBlock(level, x, y - 1, z, Tile::dirt->id);
		const int log = Tile::log2->id, leaves = ((Tile*)Tile::leaves2)->id;

		const int dir = random->nextInt(4);
		const int dx = (dir == 0) - (dir == 1);
		const int dz = (dir == 2) - (dir == 3);
		const int bendAt = height - random->nextInt(4) - 1;
		int bendLen = 3 - random->nextInt(3);

		int tx = x, tz = z, top = y;
		for (int i = 0; i < height; ++i) {
			const int yy = y + i;
			if (i >= bendAt && bendLen > 0) { tx += dx; tz += dz; --bendLen; }
			if (Gen09::canReplace(level->getTile(tx, yy, tz))) {
				placeBlock(level, tx, yy, tz, log, Log2Tile::ACACIA);
				top = yy;
			}
		}
		canopy(level, random, tx, top, tz, leaves);

		// sometimes a second, shorter branch in another direction
		int dir2 = random->nextInt(4);
		if (dir2 != dir && random->nextInt(2) == 0) {
			const int dx2 = (dir2 == 0) - (dir2 == 1);
			const int dz2 = (dir2 == 2) - (dir2 == 3);
			int bx = x, bz = z, btop = -1;
			for (int i = bendAt - 1; i < height - 1; ++i) {
				bx += dx2; bz += dz2;
				const int yy = y + i;
				if (Gen09::canReplace(level->getTile(bx, yy, bz))) {
					placeBlock(level, bx, yy, bz, log, Log2Tile::ACACIA);
					btop = yy;
				}
			}
			if (btop > 0) canopy(level, random, bx, btop, bz, leaves);
		}
		return true;
	}
private:
	void canopy(Level* level, Random* random, int cx, int cy, int cz, int leaves) {
		for (int ox = -3; ox <= 3; ++ox)
			for (int oz = -3; oz <= 3; ++oz) {
				if (std::abs(ox) == 3 && std::abs(oz) == 3) continue;
				leaf(level, cx + ox, cy, cz + oz, leaves);
			}
		for (int ox = -1; ox <= 1; ++ox)
			for (int oz = -1; oz <= 1; ++oz)
				leaf(level, cx + ox, cy + 1, cz + oz, leaves);
		leaf(level, cx + 2, cy + 1, cz, leaves); leaf(level, cx - 2, cy + 1, cz, leaves);
		leaf(level, cx, cy + 1, cz + 2, leaves); leaf(level, cx, cy + 1, cz - 2, leaves);
	}
	void leaf(Level* level, int x, int y, int z, int leaves) {
		if (y < Level::DEPTH && Gen09::canReplace(level->getTile(x, y, z)) && !Tile::isLeaf(level->getTile(x, y, z)))
			placeBlock(level, x, y, z, leaves, Leaves2Tile::ACACIA);
	}
};

/** Roofed forest dark oak: 2x2 trunk with a broad, dense, flat-ish crown. */
class DarkOakFeature : public Feature {
public:
	DarkOakFeature(bool doUpdate = false) : Feature(doUpdate) {}

	bool place(Level* level, Random* random, int x, int y, int z) {
		const int height = random->nextInt(3) + random->nextInt(2) + 6;
		if (y < 1 || y + height + 3 > Level::DEPTH) return false;
		for (int ox = 0; ox <= 1; ++ox)
			for (int oz = 0; oz <= 1; ++oz)
				if (!Gen09::isSoil(level->getTile(x + ox, y - 1, z + oz))) return false;
		for (int yy = y + 1; yy <= y + height + 2; ++yy)
			for (int xx = x - 3; xx <= x + 4; ++xx)
				for (int zz = z - 3; zz <= z + 4; ++zz)
					if (!Gen09::canReplace(level->getTile(xx, yy, zz))) return false;

		const int log = Tile::log2->id, leaves = ((Tile*)Tile::leaves2)->id;
		for (int ox = 0; ox <= 1; ++ox)
			for (int oz = 0; oz <= 1; ++oz) {
				placeBlock(level, x + ox, y - 1, z + oz, Tile::dirt->id);
				for (int i = 0; i < height; ++i)
					if (Gen09::canReplace(level->getTile(x + ox, y + i, z + oz)))
						placeBlock(level, x + ox, y + i, z + oz, log, Log2Tile::DARK_OAK);
			}

		const int top = y + height - 1;
		// crown: three layers, centred on the 2x2 trunk (x+0.5, z+0.5)
		for (int layer = -1; layer <= 1; ++layer) {
			const int r = (layer == 1) ? 2 : 3;
			for (int ox = -r; ox <= r + 1; ++ox)
				for (int oz = -r; oz <= r + 1; ++oz) {
					const float fx = ox - 0.5f, fz = oz - 0.5f;
					const float d2 = fx * fx + fz * fz;
					const float lim = (r + 0.6f) * (r + 0.6f);
					if (d2 > lim) continue;
					if (d2 > lim - 3 && random->nextInt(3) == 0) continue;
					const int xx = x + ox, yy = top + layer, zz = z + oz;
					if (yy < Level::DEPTH && level->getTile(xx, yy, zz) == 0)
						placeBlock(level, xx, yy, zz, leaves, Leaves2Tile::DARK_OAK);
				}
		}
		// a few short side branches under the crown
		for (int b = 0; b < 2 + random->nextInt(2); ++b) {
			const int dir = random->nextInt(4);
			const int dx = (dir == 0) ? 2 : (dir == 1 ? -1 : random->nextInt(2));
			const int dz = (dir == 2) ? 2 : (dir == 3 ? -1 : random->nextInt(2));
			const int by = top - 1 - random->nextInt(2);
			if (level->getTile(x + dx, by, z + dz) == 0 || Tile::isLeaf(level->getTile(x + dx, by, z + dz)))
				placeBlock(level, x + dx, by, z + dz, log, Log2Tile::DARK_OAK);
		}
		return true;
	}
};

/** Ice plains spikes: tapering packed ice columns. */
class IceSpikeFeature : public Feature {
public:
	bool place(Level* level, Random* random, int x, int y, int z) {
		while (y > 2 && level->isEmptyTile(x, y, z)) --y;
		const int ground = level->getTile(x, y, z);
		if (ground != Tile::snow->id && ground != ((Tile*)Tile::grass)->id && ground != Tile::dirt->id && ground != Tile::ice->id && ground != Tile::topSnow->id)
			return false;
		const bool big = random->nextInt(12) == 0;
		const int height = big ? 18 + random->nextInt(10) : 6 + random->nextInt(6);
		const int baseR = big ? 3 : 1 + random->nextInt(2);
		for (int i = 0; i < height && y + i < Level::DEPTH; ++i) {
			const float t = (float)i / (float)height;
			const float r = baseR * (1.0f - t) + 0.25f;
			const int ir = (int)std::ceil(r);
			for (int ox = -ir; ox <= ir; ++ox)
				for (int oz = -ir; oz <= ir; ++oz) {
					if (ox * ox + oz * oz > r * r + 0.5f) continue;
					const int t2 = level->getTile(x + ox, y + i, z + oz);
					if (t2 == 0 || t2 == Tile::snow->id || t2 == Tile::topSnow->id || t2 == Tile::ice->id || t2 == Tile::tallgrass->id)
						placeBlock(level, x + ox, y + i, z + oz, Tile::packedIce->id);
				}
		}
		// roots down into the ground
		for (int i = 1; i < 4 && y - i > 1; ++i)
			for (int ox = -baseR; ox <= baseR; ++ox)
				for (int oz = -baseR; oz <= baseR; ++oz)
					if (ox * ox + oz * oz <= baseR * baseR && level->getTile(x + ox, y - i, z + oz) != 0)
						placeBlock(level, x + ox, y - i, z + oz, Tile::packedIce->id);
		return true;
	}
};

/** Mushroom field patch: a disc of mycelium replacing grass. */
class MyceliumPatchFeature : public Feature {
public:
	bool place(Level* level, Random* random, int x, int y, int z) {
		const int r = 3 + random->nextInt(4);
		for (int ox = -r; ox <= r; ++ox)
			for (int oz = -r; oz <= r; ++oz) {
				if (ox * ox + oz * oz > r * r) continue;
				const int xx = x + ox, zz = z + oz;
				const int yy = level->getHeightmap(xx, zz) - 1;
				if (yy < 1 || level->getTile(xx, yy, zz) != ((Tile*)Tile::grass)->id) continue;
				level->setTileNoUpdate(xx, yy, zz, Tile::mycelium->id);
				if (level->getTile(xx, yy + 1, zz) == Tile::tallgrass->id) level->setTileNoUpdate(xx, yy + 1, zz, 0);
			}
		return true;
	}
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_LEVELGEN_FEATURE__Features09_H__*/
