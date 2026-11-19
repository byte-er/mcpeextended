#include "LodRenderer.h"
#include "Tesselator.h"
#include "Textures.h"
#include "TextureData.h"
#include "../../world/level/Level.h"
#include "../../world/level/LevelConstants.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/level/material/Material.h"
#include <algorithm>
#include <cmath>

static inline int lodClampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

LodRenderer::LodRenderer(Textures* textures)
:	level(NULL),
	textures(textures),
	regionsX(LEVEL_WIDTH / REGION),
	regionsZ(LEVEL_DEPTH / REGION),
	dirtyCount(0)
{
	regions.resize(regionsX * regionsZ);
}

LodRenderer::~LodRenderer() {
	freeBuffers();
}

void LodRenderer::freeBuffers() {
	for (size_t i = 0; i < regions.size(); ++i) {
		if (regions[i].vbo) glDeleteBuffers(1, &regions[i].vbo);
		regions[i].vbo = 0;
		regions[i].vertexCount = 0;
	}
}

void LodRenderer::onGraphicsReset() {
	// GL objects are gone after a context loss: forget and rebuild everything
	for (size_t i = 0; i < regions.size(); ++i) { regions[i].vbo = 0; regions[i].vertexCount = 0; }
	colorCache.clear();
	markAllDirty();
}

void LodRenderer::setLevel(Level* level) {
	this->level = level;
	freeBuffers();
	colorCache.clear();
	markAllDirty();
}

void LodRenderer::markAllDirty() {
	for (size_t i = 0; i < regions.size(); ++i) regions[i].dirty = true;
	dirtyCount = (int)regions.size();
}

void LodRenderer::markDirty(int x0, int z0, int x1, int z1) {
	const int rx0 = lodClampi(x0 / REGION, 0, regionsX - 1), rx1 = lodClampi(x1 / REGION, 0, regionsX - 1);
	const int rz0 = lodClampi(z0 / REGION, 0, regionsZ - 1), rz1 = lodClampi(z1 / REGION, 0, regionsZ - 1);
	if (x1 < 0 || z1 < 0 || x0 >= LEVEL_WIDTH || z0 >= LEVEL_DEPTH) return;
	for (int rz = rz0; rz <= rz1; ++rz)
		for (int rx = rx0; rx <= rx1; ++rx) {
			Region& r = regions[rx + rz * regionsX];
			if (!r.dirty) { r.dirty = true; ++dirtyCount; }
		}
}

float LodRenderer::farDistance() {
	// enough to see across the whole 512x512 world from anywhere in it
	return 640.0f;
}

// Average colour of a tile's top texture in terrain.png (cached)
int LodRenderer::averageTileColor(int tileId, int data) {
	const int key = (tileId << 4) | (data & 15);
	std::map<int, int>::iterator it = colorCache.find(key);
	if (it != colorCache.end()) return it->second;

	int result = 0x808080;
	Tile* tile = Tile::tiles[tileId];
	const TextureData* img = NULL;
	if (textures) img = textures->getTemporaryTextureData(textures->loadTexture("terrain.png"));

	if (tile && img && img->data && img->format == TEXF_UNCOMPRESSED_8888 && img->transparent && img->w >= 256) {
		const int tex = tile->getTexture(1, data);
		const int ts = img->w / 16; // tile size in pixels
		const int tx = (tex & 15) * ts, ty = (tex >> 4) * ts;
		long r = 0, g = 0, b = 0, n = 0;
		for (int y = 0; y < ts; ++y)
			for (int x = 0; x < ts; ++x) {
				const unsigned char* p = img->data + ((ty + y) * img->w + (tx + x)) * 4;
				if (p[3] < 32) continue;
				r += p[0]; g += p[1]; b += p[2]; ++n;
			}
		if (n > 0) result = ((int)(r / n) << 16) | ((int)(g / n) << 8) | (int)(b / n);
	} else if (tile) {
		// no texture data (e.g. compressed textures): rough material colours
		if (tile->material == Material::water) result = 0x3050c0;
		else if (tile->material == Material::sand) result = 0xd8cf9c;
		else if (tile->material == Material::leaves || tile->material == Material::plant) result = 0x8a8a8a;
		else if (tile->material == Material::snow || tile->material == Material::topSnow) result = 0xf0f0f0;
	}
	colorCache[key] = result;
	return result;
}

void LodRenderer::update(int maxRegions) {
	if (!level || dirtyCount <= 0) return;
	int built = 0;
	for (size_t i = 0; i < regions.size() && built < maxRegions; ++i) {
		if (!regions[i].dirty) continue;
		buildRegion((int)i % regionsX, (int)i / regionsX);
		++built;
	}
}

void LodRenderer::buildRegion(int rx, int rz) {
	Region& region = regions[rx + rz * regionsX];
	if (region.dirty) { region.dirty = false; --dirtyCount; }

	const int CELLS = REGION / CELL;
	const int ox = rx * REGION, oz = rz * REGION;

	// one extra ring of cells so we know the neighbours' heights for walls
	int height[CELLS + 2][CELLS + 2];
	int color[CELLS + 2][CELLS + 2];

	for (int cz = -1; cz <= CELLS; ++cz)
	for (int cx = -1; cx <= CELLS; ++cx) {
		const int bx0 = ox + cx * CELL, bz0 = oz + cz * CELL;
		if (bx0 < 0 || bz0 < 0 || bx0 >= LEVEL_WIDTH || bz0 >= LEVEL_DEPTH) {
			height[cx + 1][cz + 1] = -1; // outside the world
			color[cx + 1][cz + 1] = 0;
			continue;
		}
		int maxH = 0;
		int r = 0, g = 0, b = 0, n = 0;
		for (int dz = 0; dz < CELL; ++dz)
		for (int dx = 0; dx < CELL; ++dx) {
			const int x = bx0 + dx, z = bz0 + dz;
			int y = level->getHeightmap(x, z);
			if (y < 0) y = 0;
			if (y > Level::DEPTH) y = Level::DEPTH;
			while (y < Level::DEPTH && level->getTile(x, y, z) != 0) ++y;
			while (y > 0 && level->getTile(x, y - 1, z) == 0) --y;
			if (y <= 0) continue;
			const int id = level->getTile(x, y - 1, z);
			Tile* tile = Tile::tiles[id];
			if (!tile || id == Tile::invisible_bedrock->id) continue;

			int c = averageTileColor(id, level->getData(x, y - 1, z));
			const int tint = tile->getColor(level, x, y - 1, z);
			// slightly darker than full bright so it blends with real terrain
			float br = level->getBrightness(x, y < Level::DEPTH ? y : Level::DEPTH - 1, z) * 0.85f;
			if (br < 0.05f) br = 0.05f;
			r += (int)(((c >> 16) & 255) * ((tint >> 16) & 255) / 255 * br);
			g += (int)(((c >> 8) & 255) * ((tint >> 8) & 255) / 255 * br);
			b += (int)((c & 255) * (tint & 255) / 255 * br);
			++n;
			if (y > maxH) maxH = y;
		}
		height[cx + 1][cz + 1] = n ? maxH : 0;
		color[cx + 1][cz + 1] = n ? ((r / n) << 16) | ((g / n) << 8) | (b / n) : 0;
	}

	Tesselator& t = Tesselator::instance;
	t.begin();
	int quads = 0;
	for (int cz = 0; cz < CELLS; ++cz)
	for (int cx = 0; cx < CELLS; ++cx) {
		const int h = height[cx + 1][cz + 1];
		if (h <= 0) continue;
		const int c = color[cx + 1][cz + 1];
		const float x0 = (float)(cx * CELL), x1 = x0 + CELL;
		const float z0 = (float)(cz * CELL), z1 = z0 + CELL;
		const float y = (float)h;

		t.color(c);
		t.vertex(x0, y, z0); t.vertex(x0, y, z1); t.vertex(x1, y, z1); t.vertex(x1, y, z0);
		++quads;

		// walls down to lower neighbours (darker, like block side shading)
		const int nb[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
		for (int k = 0; k < 4; ++k) {
			const int nh = height[cx + 1 + nb[k][0]][cz + 1 + nb[k][1]];
			if (nh < 0 || nh >= h) continue; // outside the world or not lower
			const float shade = (k < 2) ? 0.6f : 0.8f;
			t.color((int)(((c >> 16) & 255) * shade), (int)(((c >> 8) & 255) * shade), (int)((c & 255) * shade));
			const float yl = (float)nh;
			if (k == 0)      { t.vertex(x0, y, z0); t.vertex(x0, yl, z0); t.vertex(x0, yl, z1); t.vertex(x0, y, z1); }
			else if (k == 1) { t.vertex(x1, y, z1); t.vertex(x1, yl, z1); t.vertex(x1, yl, z0); t.vertex(x1, y, z0); }
			else if (k == 2) { t.vertex(x1, y, z0); t.vertex(x1, yl, z0); t.vertex(x0, yl, z0); t.vertex(x0, y, z0); }
			else             { t.vertex(x0, y, z1); t.vertex(x0, yl, z1); t.vertex(x1, yl, z1); t.vertex(x1, y, z1); }
			++quads;
		}
	}

	if (quads == 0) {
		t.end(true, 0);
		region.vertexCount = 0;
		return;
	}
	if (!region.vbo) glGenBuffers(1, &region.vbo);
	RenderChunk rc = t.end(true, region.vbo);
	region.vertexCount = rc.vertexCount;
}

void LodRenderer::render(double camX, double camY, double camZ, float innerHalf) {
	if (!level) return;

	glDisable2(GL_TEXTURE_2D);
	glDisable2(GL_CULL_FACE);
	glDisable2(GL_BLEND);
	glDisable2(GL_ALPHA_TEST);

	for (int rz = 0; rz < regionsZ; ++rz)
	for (int rx = 0; rx < regionsX; ++rx) {
		const Region& r = regions[rx + rz * regionsX];
		if (!r.vbo || r.vertexCount <= 0) continue;
		const double x0 = rx * REGION, z0 = rz * REGION;
		const double x1 = x0 + REGION, z1 = z0 + REGION;
		// completely inside the square where real chunks render: skip
		if (x0 >= camX - innerHalf && x1 <= camX + innerHalf &&
			z0 >= camZ - innerHalf && z1 <= camZ + innerHalf)
			continue;

		glPushMatrix2();
		glTranslatef2((float)(x0 - camX), (float)(-camY), (float)(z0 - camZ));
		drawArrayVTC(r.vbo, r.vertexCount, 24);
		glPopMatrix2();
	}

	glEnable2(GL_CULL_FACE);
	glEnable2(GL_TEXTURE_2D);
}
