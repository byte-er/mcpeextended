#ifndef NET_MINECRAFT_CLIENT_RENDERER__LodRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER__LodRenderer_H__

#include <vector>
#include <map>
#include "gles.h"

class Level;
class Textures;

/**
 * Level-of-detail terrain ("distant horizons" style), three quality levels:
 *
 *   LEVEL_NEAR  1x1 block cells, textured with the real block textures
 *               (top faces + exposed sides). Used just outside the area
 *               covered by real chunks, so the hand-over is hardly visible.
 *   LEVEL_MID   2x2 block cells, flat shaded with averaged block colours.
 *   LEVEL_FAR   4x4 block cells, flat shaded.
 *
 * The world is split into REGION x REGION block regions; each region keeps a
 * VBO per level, built on demand (nearest first) and freed when no longer
 * needed. Regions whose chunk columns are all built and rendered as real
 * chunks are skipped; everywhere else (including chunks that are still being
 * rebuilt) the LOD shows, so there are never holes in the world.
 */
class LodRenderer
{
public:
	enum { LEVEL_NEAR = 0, LEVEL_MID = 1, LEVEL_FAR = 2, NUM_LEVELS = 3 };
	static const int REGION = 32;   // blocks per region (each axis)

	LodRenderer(Textures* textures);
	~LodRenderer();

	void setLevel(Level* level);
	void markDirty(int x0, int z0, int x1, int z1);
	void markAllDirty();

	/**
	 * Builds what is needed this frame (bounded work) and draws.
	 * camX/Y/Z: interpolated camera position. innerHalf: half size of the
	 * square around the camera covered by real chunks. columnReady: one flag
	 * per 16x16 chunk column of the level (CHUNK_CACHE_WIDTH^2), true when
	 * every real chunk of that column is built and rendered.
	 */
	void render(double camX, double camY, double camZ, float innerHalf, const std::vector<char>& columnReady);

	/** Distance up to which LOD terrain is visible (for fog / far plane). */
	static float farDistance();

	void onGraphicsReset();

private:
	struct Region {
		GLuint vbo[NUM_LEVELS];
		int vertexCount[NUM_LEVELS];
		bool dirty[NUM_LEVELS];
		Region() { for (int i = 0; i < NUM_LEVELS; ++i) { vbo[i] = 0; vertexCount[i] = 0; dirty[i] = true; } }
	};

	void buildRegion(int rx, int rz, int level);
	void buildTextured(int rx, int rz, Region& region);
	void buildFlat(int rx, int rz, int cell, Region& region);
	int  surfaceY(int x, int z, int& tileId);
	int  averageTileColor(int tileId, int data);
	int  textureFor(int tileId, int face, int data);
	void freeBuffers();
	void freeLevel(Region& r, int level);

	Level* level;
	Textures* textures;
	int regionsX, regionsZ;
	std::vector<Region> regions;
	std::map<int, int> colorCache; // (id << 4 | data) -> 0xRRGGBB
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__LodRenderer_H__*/
