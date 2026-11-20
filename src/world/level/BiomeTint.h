#ifndef NET_MINECRAFT_WORLD_LEVEL__BiomeTint_H__
#define NET_MINECRAFT_WORLD_LEVEL__BiomeTint_H__

class Level;

/**
 * Biome colours for grass, tall grass and leaves, like Java / PE 0.9.0.
 *
 * Each biome gets its PE 0.9.0 / Java 1.7 temperature and rainfall, looked
 * up in the 256x256 colormaps (images/misc/grasscolor.png and
 * foliagecolor.png), then blended over 7x7 columns so biome borders fade.
 *
 * Values are computed once per 16x16 column block and cached; the cache has
 * its own BiomeSource so it never disturbs world generation buffers.
 */
class BiomeTint
{
public:
	/** colormaps: 256*256 pixels, RGBA bytes. Copied. */
	static void setColormaps(const unsigned char* grassRGBA, const unsigned char* foliageRGBA);
	/** Called when the rendered level changes (NULL to clear). */
	static void setLevel(Level* level);

	/**
	 * Server worlds: biome ids come from the server (Java 1.8 ids). `fn`
	 * returns the id at a local block column, or -1 if unknown.
	 */
	typedef int (*BiomeIdFn)(int x, int z, void* ctx);
	static void setExternalSource(BiomeIdFn fn, void* ctx);
	/** Drops cached colours around 16x16 block (bx, bz) after its biomes changed. */
	static void invalidateBlock(int bx, int bz);
	static void clearCache();

	static int grass(int x, int z);
	static int foliage(int x, int z);

	static const int DEFAULT_GRASS   = 0x7fb238;
	static const int DEFAULT_FOLIAGE = 0x48b518;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__BiomeTint_H__*/
