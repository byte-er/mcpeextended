#ifndef NET_MINECRAFT_WORLD_LEVEL_TILE__Tiles09_H__
#define NET_MINECRAFT_WORLD_LEVEL_TILE__Tiles09_H__

// Blocks introduced with Minecraft: Pocket Edition 0.9.0 (block ids match PE).
// Textures live in previously unused slots of terrain.png (see TEX_* below).

#include "Tile.h"
#include "Bush.h"
#include "HeavyTile.h"
#include "TreeTile.h"
#include "LeafTile.h"
#include "../material/Material.h"
#include "../FoliageColor.h"

namespace Tex09 {
	// terrain.png atlas slots (row * 16 + column)
	const int PODZOL_TOP        = 30;
	const int PODZOL_SIDE       = 153;
	const int RED_SAND          = 170;
	const int PACKED_ICE        = 171;
	const int MYCELIUM_TOP      = 172;
	const int MYCELIUM_SIDE     = 173;
	const int HARDENED_CLAY     = 180;
	const int STAINED_CLAY      = 181; // white base, tinted per colour
	const int FLOWER_FIRST      = 184; // 184..191: orchid, allium, bluet, 4 tulips, daisy
	const int ACACIA_LOG        = 199;
	const int ACACIA_LOG_TOP    = 200;
	const int DARK_OAK_LOG      = 201;
	const int DARK_OAK_LOG_TOP  = 202;
	const int ACACIA_PLANKS     = 203;
	const int DARK_OAK_PLANKS   = 215;
	const int ACACIA_LEAVES     = 216;
	const int DARK_OAK_LEAVES   = 217;
	const int COAL_BLOCK        = 231;

	const int OAK_PLANKS        = 4;
	const int DIRT              = 2;
}

/** Grass-like block with distinct top / side / bottom textures that drops dirt. */
class TopSideTile : public Tile
{
public:
	TopSideTile(int id, int top, int side, int bottom, const Material* mat)
	:	Tile(id, side, mat), texTop(top), texSide(side), texBottom(bottom) {}

	int getTexture(int face, int data) {
		if (face == 1) return texTop;
		if (face == 0) return texBottom;
		return texSide;
	}
	int getResource(int data, Random* random) { return Tile::dirt->id; }
private:
	int texTop, texSide, texBottom;
};

/** Sand (data 0) and red sand (data 1), PE id 12. */
class SandVariantTile : public HeavyTile
{
public:
	static const int TYPE_SAND = 0;
	static const int TYPE_RED  = 1;

	SandVariantTile(int id, int tex) : HeavyTile(id, tex) {}

	int getTexture(int face, int data) {
		return (data & 1) == TYPE_RED ? Tex09::RED_SAND : tex;
	}
protected:
	int getSpawnResourcesAuxValue(int data) { return data & 1; }
};

/** Stained clay, PE id 159. One texture, tinted per dye colour (data 0..15). */
class StainedClayTile : public Tile
{
public:
	StainedClayTile(int id) : Tile(id, Tex09::STAINED_CLAY, Material::stone) {}

	int getColor(LevelSource* level, int x, int y, int z) {
		return getItemColor(level->getData(x, y, z));
	}
	int getItemColor(int data) { return COLORS[data & 15]; }

	static const int COLORS[16];
protected:
	int getSpawnResourcesAuxValue(int data) { return data & 15; }
};

/** Flowers, PE id 38: 0 = rose/poppy (original), 1..8 = 0.9.0 flowers. */
class FlowerTile : public Bush
{
public:
	static const int TYPE_COUNT = 9;
	FlowerTile(int id, int roseTex) : Bush(id, roseTex) {}

	int getTexture(int face, int data) {
		data &= 15;
		if (data <= 0 || data >= TYPE_COUNT) return tex;
		return Tex09::FLOWER_FIRST + (data - 1);
	}
protected:
	int getSpawnResourcesAuxValue(int data) {
		data &= 15;
		return data < TYPE_COUNT ? data : 0;
	}
};

/** Planks with wood type in data: 0 oak, 4 acacia, 5 dark oak (1..3 fall back to oak). */
class PlanksTile : public Tile
{
public:
	static const int ACACIA   = 4;
	static const int DARK_OAK = 5;
	PlanksTile(int id) : Tile(id, Tex09::OAK_PLANKS, Material::wood) {}

	int getTexture(int face, int data) {
		if (data == ACACIA)   return Tex09::ACACIA_PLANKS;
		if (data == DARK_OAK) return Tex09::DARK_OAK_PLANKS;
		return tex;
	}
protected:
	int getSpawnResourcesAuxValue(int data) { return (data == ACACIA || data == DARK_OAK) ? data : 0; }
};

/** Acacia (data&3 == 0) and dark oak (1) logs, PE id 162. */
class Log2Tile : public TreeTile
{
public:
	static const int ACACIA   = 0;
	static const int DARK_OAK = 1;
	Log2Tile(int id) : TreeTile(id) { tex = Tex09::ACACIA_LOG; }

	int getResource(int data, Random* random) { return id; }

	int getTexture(int face, int data) {
		const bool dark = (data & 3) == DARK_OAK;
		if (face == 0 || face == 1) return dark ? Tex09::DARK_OAK_LOG_TOP : Tex09::ACACIA_LOG_TOP;
		return dark ? Tex09::DARK_OAK_LOG : Tex09::ACACIA_LOG;
	}
protected:
	int getSpawnResourcesAuxValue(int data) { return data & 1; }
};

/** Acacia / dark oak leaves, PE id 161. Always rendered cut-out (no opaque variant). */
class Leaves2Tile : public LeafTile
{
public:
	static const int ACACIA   = 0;
	static const int DARK_OAK = 1;
	Leaves2Tile(int id) : LeafTile(id, Tex09::ACACIA_LEAVES) { allowSame = true; }

	int getTexture(int face, int data) {
		return (data & LEAF_TYPE_MASK) == DARK_OAK ? Tex09::DARK_OAK_LEAVES : Tex09::ACACIA_LEAVES;
	}
	bool isSolidRender() { return false; }
	int getRenderLayer() { return Tile::RENDERLAYER_ALPHATEST; }

	int getColor(LevelSource* level, int x, int y, int z) { return BiomeTint::foliage(x, z); }
	int getItemColor(int data) { return FoliageColor::getDefaultColor(); }

	int getResource(int data, Random* random) { return Tile::sapling->id; }
protected:
	// no acacia/dark oak saplings yet: drop oak saplings
	int getSpawnResourcesAuxValue(int data) { return 0; }
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_TILE__Tiles09_H__*/
