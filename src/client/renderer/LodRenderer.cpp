#include "LodRenderer.h"
#include "Tesselator.h"
#include "Textures.h"
#include "TextureData.h"
#include "../../world/level/Level.h"
#include "../../world/level/LevelConstants.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/level/tile/LeafTile.h"
#include "../../world/level/material/Material.h"
#include <algorithm>
#include <cmath>

static inline int lodClampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

LodRenderer::LodRenderer(Textures* textures)
:	level(NULL),
	textures(textures),
	regionsX(LEVEL_WIDTH / REGION),
	regionsZ(LEVEL_DEPTH / REGION)
{
	regions.resize(regionsX * regionsZ);
}

LodRenderer::~LodRenderer() {
	freeBuffers();
}

void LodRenderer::freeLevel(Region& r, int l) {
	if (r.vbo[l]) glDeleteBuffers(1, &r.vbo[l]);
	r.vbo[l] = 0;
	r.vertexCount[l] = 0;
	r.dirty[l] = true;
}

void LodRenderer::freeBuffers() {
	for (size_t i = 0; i < regions.size(); ++i)
		for (int l = 0; l < NUM_LEVELS; ++l) freeLevel(regions[i], l);
}

void LodRenderer::onGraphicsReset() {
	// GL objects are gone after a context loss: forget them (don't delete)
	for (size_t i = 0; i < regions.size(); ++i)
		for (int l = 0; l < NUM_LEVELS; ++l) { regions[i].vbo[l] = 0; regions[i].vertexCount[l] = 0; regions[i].dirty[l] = true; }
	colorCache.clear();
}

void LodRenderer::setLevel(Level* level) {
	this->level = level;
	freeBuffers();
	colorCache.clear();
}

void LodRenderer::markAllDirty() {
	for (size_t i = 0; i < regions.size(); ++i)
		for (int l = 0; l < NUM_LEVELS; ++l) regions[i].dirty[l] = true;
}

void LodRenderer::markDirty(int x0, int z0, int x1, int z1) {
	if (x1 < 0 || z1 < 0 || x0 >= LEVEL_WIDTH || z0 >= LEVEL_DEPTH) return;
	const int rx0 = lodClampi(x0 / REGION, 0, regionsX - 1), rx1 = lodClampi(x1 / REGION, 0, regionsX - 1);
	const int rz0 = lodClampi(z0 / REGION, 0, regionsZ - 1), rz1 = lodClampi(z1 / REGION, 0, regionsZ - 1);
	for (int rz = rz0; rz <= rz1; ++rz)
		for (int rx = rx0; rx <= rx1; ++rx)
			for (int l = 0; l < NUM_LEVELS; ++l) regions[rx + rz * regionsX].dirty[l] = true;
}

float LodRenderer::farDistance() {
	// enough to see across the whole 512x512 world from anywhere in it
	return 640.0f;
}

// Texture index used for LOD faces; leaves use their opaque variant so the
// single-layer LOD surface has no holes.
int LodRenderer::textureFor(int tileId, int face, int data) {
	Tile* tile = Tile::tiles[tileId];
	int tex = tile->getTexture(face, data);
	if ((tile == (Tile*)Tile::leaves) && !tile->isSolidRender()) tex += 1;
	return tex;
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
		const int tex = textureFor(tileId, 1, data);
		const int ts = img->w / 16;
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
		if (tile->material == Material::water) result = 0x3050c0;
		else if (tile->material == Material::sand) result = 0xd8cf9c;
		else if (tile->material == Material::snow || tile->material == Material::topSnow) result = 0xf0f0f0;
	}
	colorCache[key] = result;
	return result;
}

// Height of the visible surface (top of the highest solid block or liquid,
// ignoring plants / torches / snow layers), and that block's id.
int LodRenderer::surfaceY(int x, int z, int& tileId) {
	// infinite worlds: never generate terrain just for the distant view
	if (!level->isChunkLoaded(x >> 4, z >> 4)) { tileId = 0; return 0; }
	int y = level->getHeightmap(x, z);
	if (y < 0) y = 0;
	if (y > Level::DEPTH) y = Level::DEPTH;
	while (y < Level::DEPTH && level->getTile(x, y, z) != 0) ++y;
	while (y > 0) {
		const int id = level->getTile(x, y - 1, z);
		Tile* t = id ? Tile::tiles[id] : NULL;
		if (t && id != Tile::invisible_bedrock->id &&
			(Tile::solid[id] || Tile::isLeaf(id) || t->material == Material::water || t->material == Material::lava)) {
			tileId = id;
			return y;
		}
		--y;
	}
	tileId = 0;
	return 0;
}

void LodRenderer::buildRegion(int rx, int rz, int l) {
	Region& region = regions[rx + rz * regionsX];
	region.dirty[l] = false;
	if (l == LEVEL_NEAR) buildTextured(rx, rz, region);
	else buildFlat(rx, rz, l == LEVEL_MID ? 2 : 4, region);
}

static void lodShade(Tesselator& t, int tint, float br) {
	t.color((int)(((tint >> 16) & 255) * br), (int)(((tint >> 8) & 255) * br), (int)((tint & 255) * br));
}

// LEVEL_NEAR: every column becomes a textured top face plus textured walls
// down to lower neighbours - a cave-less, interior-less copy of the terrain.
void LodRenderer::buildTextured(int rx, int rz, Region& region) {
	const int N = REGION;
	const int ox = rx * REGION, oz = rz * REGION;
	int h[N + 2][N + 2];
	int id[N + 2][N + 2];
	for (int dz = -1; dz <= N; ++dz)
	for (int dx = -1; dx <= N; ++dx) {
		const int x = ox + dx, z = oz + dz;
		if (x < 0 || z < 0 || x >= LEVEL_WIDTH || z >= LEVEL_DEPTH) { h[dx + 1][dz + 1] = -1; id[dx + 1][dz + 1] = 0; continue; }
		int tid;
		h[dx + 1][dz + 1] = surfaceY(x, z, tid);
		id[dx + 1][dz + 1] = tid;
	}

	const float us = 1.0f / 16.0f;
	Tesselator& t = Tesselator::instance;
	t.begin();
	int quads = 0;
	for (int dz = 0; dz < N; ++dz)
	for (int dx = 0; dx < N; ++dx) {
		const int y = h[dx + 1][dz + 1];
		const int tid = id[dx + 1][dz + 1];
		if (y <= 0 || !tid) continue;
		const int x = ox + dx, z = oz + dz;
		Tile* tile = Tile::tiles[tid];
		const int data = level->getData(x, y - 1, z);
		const int tint = tile->getColor(level, x, y - 1, z);
		const float br = level->getBrightness(x, y < Level::DEPTH ? y : Level::DEPTH - 1, z);
		const bool liquid = tile->material == Material::water || tile->material == Material::lava;
		const float top = liquid ? (float)y - 0.1f : (float)y;

		// top face
		{
			const int tex = textureFor(tid, 1, data);
			const float u0 = (tex & 15) * us, v0 = (tex >> 4) * us;
			const float u1 = u0 + us * 0.999f, v1 = v0 + us * 0.999f;
			lodShade(t, tint, br);
			const float x0 = (float)dx, x1 = x0 + 1, z0 = (float)dz, z1 = z0 + 1;
			t.vertexUV(x0, top, z0, u0, v0); t.vertexUV(x0, top, z1, u0, v1);
			t.vertexUV(x1, top, z1, u1, v1); t.vertexUV(x1, top, z0, u1, v0);
			++quads;
		}
		if (liquid) continue;

		// walls towards lower neighbours, one textured quad per block (max 24)
		static const int nb[4][3] = { { -1, 0, 4 }, { 1, 0, 5 }, { 0, -1, 2 }, { 0, 1, 3 } };
		for (int k = 0; k < 4; ++k) {
			const int nh = h[dx + 1 + nb[k][0]][dz + 1 + nb[k][1]];
			if (nh < 0 || nh >= y) continue;
			const float shade = br * ((k < 2) ? 0.6f : 0.8f);
			const int bottom = std::max(nh, y - 24);
			for (int yy = y - 1; yy >= bottom; --yy) {
				const int bid = level->getTile(x, yy, z);
				if (!bid || !Tile::tiles[bid]) continue;
				const int bdata = level->getData(x, yy, z);
				const int tex = textureFor(bid, nb[k][2], bdata);
				// grass sides etc. aren't tinted; only tint the top block's own side if it's leaves
				const int sideTint = Tile::isLeaf(bid) ? Tile::tiles[bid]->getColor(level, x, yy, z) : 0xffffff;
				lodShade(t, sideTint, shade);
				const float u0 = (tex & 15) * us, v0 = (tex >> 4) * us;
				const float u1 = u0 + us * 0.999f, v1 = v0 + us * 0.999f;
				const float y0 = (float)yy, y1 = (float)(yy + 1);
				// stretch the last quad down to the neighbour on deep cliffs
				const float yb = (yy == bottom) ? (float)nh : y0;
				const float x0 = (float)dx, x1 = x0 + 1, z0 = (float)dz, z1 = z0 + 1;
				if (k == 0)      { t.vertexUV(x0, y1, z0, u0, v0); t.vertexUV(x0, yb, z0, u0, v1); t.vertexUV(x0, yb, z1, u1, v1); t.vertexUV(x0, y1, z1, u1, v0); }
				else if (k == 1) { t.vertexUV(x1, y1, z1, u0, v0); t.vertexUV(x1, yb, z1, u0, v1); t.vertexUV(x1, yb, z0, u1, v1); t.vertexUV(x1, y1, z0, u1, v0); }
				else if (k == 2) { t.vertexUV(x1, y1, z0, u0, v0); t.vertexUV(x1, yb, z0, u0, v1); t.vertexUV(x0, yb, z0, u1, v1); t.vertexUV(x0, y1, z0, u1, v0); }
				else             { t.vertexUV(x0, y1, z1, u0, v0); t.vertexUV(x0, yb, z1, u0, v1); t.vertexUV(x1, yb, z1, u1, v1); t.vertexUV(x1, y1, z1, u1, v0); }
				++quads;
			}
		}
	}

	if (quads == 0) { t.end(true, 0); region.vertexCount[LEVEL_NEAR] = 0; return; }
	if (!region.vbo[LEVEL_NEAR]) glGenBuffers(1, &region.vbo[LEVEL_NEAR]);
	RenderChunk rc = t.end(true, region.vbo[LEVEL_NEAR]);
	region.vertexCount[LEVEL_NEAR] = rc.vertexCount;
}

// LEVEL_MID / LEVEL_FAR: cell x cell block heightfield with averaged colours
void LodRenderer::buildFlat(int rx, int rz, int cell, Region& region) {
	const int l = (cell == 2) ? LEVEL_MID : LEVEL_FAR;
	const int CELLS = REGION / cell;
	const int ox = rx * REGION, oz = rz * REGION;

	std::vector<int> height((CELLS + 2) * (CELLS + 2)), color((CELLS + 2) * (CELLS + 2));
	#define H(cx, cz) height[((cx) + 1) * (CELLS + 2) + (cz) + 1]
	#define C(cx, cz) color[((cx) + 1) * (CELLS + 2) + (cz) + 1]

	for (int cz = -1; cz <= CELLS; ++cz)
	for (int cx = -1; cx <= CELLS; ++cx) {
		const int bx0 = ox + cx * cell, bz0 = oz + cz * cell;
		if (bx0 < 0 || bz0 < 0 || bx0 >= LEVEL_WIDTH || bz0 >= LEVEL_DEPTH) { H(cx, cz) = -1; C(cx, cz) = 0; continue; }
		int maxH = 0, r = 0, g = 0, b = 0, n = 0;
		for (int dz = 0; dz < cell; ++dz)
		for (int dx = 0; dx < cell; ++dx) {
			const int x = bx0 + dx, z = bz0 + dz;
			int tid;
			const int y = surfaceY(x, z, tid);
			if (y <= 0 || !tid) continue;
			Tile* tile = Tile::tiles[tid];
			const int c = averageTileColor(tid, level->getData(x, y - 1, z));
			const int tint = tile->getColor(level, x, y - 1, z);
			float br = level->getBrightness(x, y < Level::DEPTH ? y : Level::DEPTH - 1, z);
			if (br < 0.05f) br = 0.05f;
			r += (int)(((c >> 16) & 255) * ((tint >> 16) & 255) / 255 * br);
			g += (int)(((c >> 8) & 255) * ((tint >> 8) & 255) / 255 * br);
			b += (int)((c & 255) * (tint & 255) / 255 * br);
			++n;
			if (y > maxH) maxH = y;
		}
		H(cx, cz) = n ? maxH : 0;
		C(cx, cz) = n ? ((r / n) << 16) | ((g / n) << 8) | (b / n) : 0;
	}

	Tesselator& t = Tesselator::instance;
	t.begin();
	int quads = 0;
	for (int cz = 0; cz < CELLS; ++cz)
	for (int cx = 0; cx < CELLS; ++cx) {
		const int hh = H(cx, cz);
		if (hh <= 0) continue;
		const int c = C(cx, cz);
		const float x0 = (float)(cx * cell), x1 = x0 + cell;
		const float z0 = (float)(cz * cell), z1 = z0 + cell;
		const float y = (float)hh;
		t.color(c);
		t.vertex(x0, y, z0); t.vertex(x0, y, z1); t.vertex(x1, y, z1); t.vertex(x1, y, z0);
		++quads;
		const int nb[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
		for (int k = 0; k < 4; ++k) {
			const int nh = H(cx + nb[k][0], cz + nb[k][1]);
			if (nh < 0 || nh >= hh) continue;
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
	#undef H
	#undef C

	if (quads == 0) { t.end(true, 0); region.vertexCount[l] = 0; return; }
	if (!region.vbo[l]) glGenBuffers(1, &region.vbo[l]);
	RenderChunk rc = t.end(true, region.vbo[l]);
	region.vertexCount[l] = rc.vertexCount;
}

void LodRenderer::render(double camX, double camY, double camZ, float innerHalf, const std::vector<char>& columnReady) {
	if (!level) return;

	// ---- decide what each region needs ----
	const float nearLimit = innerHalf + 48.0f;   // textured, block resolution
	const float midLimit  = innerHalf + 176.0f;  // 2x2 cells, beyond: 4x4
	struct Want { int idx; int level; float dist; };
	std::vector<Want> draw;
	draw.reserve(regions.size());
	const int cw = CHUNK_CACHE_WIDTH;
	for (int rz = 0; rz < regionsZ; ++rz)
	for (int rx = 0; rx < regionsX; ++rx) {
		// skip when every chunk column of the region is rendered for real
		bool covered = !columnReady.empty();
		const int per = REGION / 16;
		for (int cz = rz * per; covered && cz < (rz + 1) * per; ++cz)
			for (int cx = rx * per; covered && cx < (rx + 1) * per; ++cx)
				if (cx >= cw || cz >= cw || !columnReady[cx + cz * cw]) covered = false;
		const int idx = rx + rz * regionsX;
		const double x0 = rx * REGION, z0 = rz * REGION;
		const double ddx = std::max(std::max(x0 - camX, camX - (x0 + REGION)), 0.0);
		const double ddz = std::max(std::max(z0 - camZ, camZ - (z0 + REGION)), 0.0);
		const float dist = (float)std::max(ddx, ddz);
		const int want = dist <= nearLimit ? LEVEL_NEAR : (dist <= midLimit ? LEVEL_MID : LEVEL_FAR);
		Region& r = regions[idx];
		// release finer meshes that are no longer needed
		for (int l = 0; l < want; ++l) if (r.vbo[l]) freeLevel(r, l);
		if (covered) continue;
		Want w = { idx, want, dist };
		draw.push_back(w);
	}

	// ---- bounded rebuilds, nearest first; coarse fallback while waiting ----
	std::sort(draw.begin(), draw.end(), [](const Want& a, const Want& b) { return a.dist < b.dist; });
	int budget = 3;
	for (size_t i = 0; i < draw.size() && budget > 0; ++i) {
		Region& r = regions[draw[i].idx];
		if (r.dirty[draw[i].level]) { buildRegion(draw[i].idx % regionsX, draw[i].idx / regionsX, draw[i].level); --budget; }
	}
	// make sure every drawn region has at least the far level (cheap)
	int farBudget = 8;
	for (size_t i = 0; i < draw.size() && farBudget > 0; ++i) {
		Region& r = regions[draw[i].idx];
		if (!r.vbo[draw[i].level] && r.dirty[LEVEL_FAR]) { buildRegion(draw[i].idx % regionsX, draw[i].idx / regionsX, LEVEL_FAR); --farBudget; }
	}

	// ---- draw ----
	glDisable2(GL_CULL_FACE);
	glDisable2(GL_BLEND);
	for (int pass = 0; pass < 2; ++pass) {
		const bool textured = pass == 0;
		if (textured) {
			textures->loadAndBindTexture("terrain.png");
			glEnable2(GL_TEXTURE_2D);
			glEnable2(GL_ALPHA_TEST);
		} else {
			glDisable2(GL_TEXTURE_2D);
			glDisable2(GL_ALPHA_TEST);
		}
		for (size_t i = 0; i < draw.size(); ++i) {
			const Region& r = regions[draw[i].idx];
			int l = draw[i].level;
			// fall back to a coarser mesh while the wanted one is being built
			while (l < NUM_LEVELS && (!r.vbo[l] || r.vertexCount[l] <= 0)) ++l;
			if (l >= NUM_LEVELS) continue;
			if ((l == LEVEL_NEAR) != textured) continue;
			const int rx = draw[i].idx % regionsX, rz = draw[i].idx / regionsX;
			glPushMatrix2();
			glTranslatef2((float)(rx * REGION - camX), (float)(-camY), (float)(rz * REGION - camZ));
			drawArrayVTC(r.vbo[l], r.vertexCount[l], 24);
			glPopMatrix2();
		}
	}
	glEnable2(GL_CULL_FACE);
	glEnable2(GL_TEXTURE_2D);
}
