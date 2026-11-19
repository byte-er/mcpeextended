#include "LargeFeature.h"
#include "../Level.h"

LargeFeature::LargeFeature() :	radius(8)
{
}

LargeFeature::~LargeFeature()
{
}

void LargeFeature::apply( ChunkSource* chunkSource, Level* level, int xOffs, int zOffs, unsigned char* blocks, int blocksSize )
{
	int r = radius;

	random.setSeed(level->getSeed());
	const unsigned int xScale = (unsigned int)(random.nextLong() / 2 * 2 + 1);
	const unsigned int zScale = (unsigned int)(random.nextLong() / 2 * 2 + 1);

	for (int x = xOffs - r; x <= xOffs + r; x++) {
		for (int z = zOffs - r; z <= zOffs + r; z++) {
			// 32 bit wrap-around made explicit (signed overflow is undefined)
			const int h = (int)((unsigned int)x * xScale + (unsigned int)z * zScale);
			random.setSeed(h ^ level->getSeed());
			addFeature(level, x, z, xOffs, zOffs, blocks, blocksSize);
		}
	}
}

