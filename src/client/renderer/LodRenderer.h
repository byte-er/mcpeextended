#ifndef NET_MINECRAFT_CLIENT_RENDERER__LodRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER__LodRenderer_H__

#include <vector>
#include <map>
#include "gles.h"

class Level;
class Textures;

/**
 * Level-of-detail terrain ("distant horizons" style).
 *
 * The (finite) world is covered by a coarse heightfield: one cell per
 * CELL x CELL blocks, coloured with the average texture colour of the top
 * block (including biome tint and light). Cells are grouped into regions of
 * REGION x REGION blocks, each stored in its own VBO. Regions that lie
 * completely inside the area covered by real chunks are skipped, so the LOD
 * only shows beyond the normal render distance.
 */
class LodRenderer
{
public:
	static const int CELL = 4;      // blocks per LOD cell (each axis)
	static const int REGION = 32;   // blocks per region (each axis)

	LodRenderer(Textures* textures);
	~LodRenderer();

	void setLevel(Level* level);
	void markDirty(int x0, int z0, int x1, int z1);
	void markAllDirty();

	/** Rebuilds a few dirty regions; call once per frame. */
	void update(int maxRegions);

	/**
	 * Draws regions not fully covered by real chunks.
	 * camX/Y/Z: interpolated camera position; innerHalf: half size of the
	 * square around the camera where real chunks are rendered.
	 */
	void render(double camX, double camY, double camZ, float innerHalf);

	/** Distance up to which LOD terrain is visible (for fog / far plane). */
	static float farDistance();

	void onGraphicsReset();

private:
	struct Region {
		GLuint vbo;
		int vertexCount;
		bool dirty;
		Region() : vbo(0), vertexCount(0), dirty(true) {}
	};

	void buildRegion(int rx, int rz);
	int averageTileColor(int tileId, int data);
	void freeBuffers();

	Level* level;
	Textures* textures;
	int regionsX, regionsZ;
	std::vector<Region> regions;
	std::map<int, int> colorCache; // (id << 4 | data) -> 0xRRGGBB
	int dirtyCount;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__LodRenderer_H__*/
