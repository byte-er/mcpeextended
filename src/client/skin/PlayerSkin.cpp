#include "PlayerSkin.h"
#include "../Minecraft.h"
#include "../renderer/Textures.h"
#include "../../AppPlatform.h"
#include <map>
#include <cstring>

namespace {
	struct Entry { std::vector<unsigned char> rgba; bool slim; };
	std::map<std::string, Entry> g_skins;
	std::vector<unsigned char> g_localEagler;
	bool g_localSlim = false;
	bool g_localCustom = false;

	// legacy 64x32 skins: copy the right arm/leg to the left slots, mirrored
	void copyMirrored(unsigned char* img, int sx, int sy, int w, int h, int dx, int dy) {
		for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			memcpy(img + ((dy + y) * 64 + dx + (w - 1 - x)) * 4, img + ((sy + y) * 64 + sx + x) * 4, 4);
	}
	void expandLegacy(unsigned char* img) {
		// leg: top/bottom faces then front/sides/back strips
		copyMirrored(img, 4, 16, 4, 4, 20, 48);  copyMirrored(img, 8, 16, 4, 4, 24, 48);
		copyMirrored(img, 0, 20, 4, 12, 24, 52); copyMirrored(img, 4, 20, 4, 12, 20, 52);
		copyMirrored(img, 8, 20, 4, 12, 16, 52); copyMirrored(img, 12, 20, 4, 12, 28, 52);
		// arm
		copyMirrored(img, 44, 16, 4, 4, 36, 48); copyMirrored(img, 48, 16, 4, 4, 40, 48);
		copyMirrored(img, 40, 20, 4, 12, 40, 52); copyMirrored(img, 44, 20, 4, 12, 36, 52);
		copyMirrored(img, 48, 20, 4, 12, 32, 52); copyMirrored(img, 52, 20, 4, 12, 44, 52);
	}
}

std::string PlayerSkin::customSkinPath(Minecraft* mc)
{
	return mc->externalStoragePath + "/games/com.mojang/skin.png";
}

bool PlayerSkin::loadImage64(Minecraft* mc, const std::string& name, bool asset, std::vector<unsigned char>& out)
{
	if (!mc->platform()) return false;
	TextureData t = mc->platform()->loadTexture(name, asset);
	bool ok = t.data && t.w == 64 && (t.h == 64 || t.h == 32) && t.format == TEXF_UNCOMPRESSED_8888;
	if (ok) {
		out.assign(64 * 64 * 4, 0);
		memcpy(&out[0], t.data, 64 * t.h * 4);
		if (t.h == 32) expandLegacy(&out[0]);
	}
	if (t.data && !t.memoryHandledExternally) delete[] t.data;
	return ok;
}

void PlayerSkin::set(Minecraft* mc, const std::string& name, const unsigned char* rgba, bool slim)
{
	Entry& e = g_skins[name];
	e.rgba.assign(rgba, rgba + 64 * 64 * 4);
	e.slim = slim;
	if (mc && mc->textures) mc->textures->releaseTexture(name);
}

bool PlayerSkin::textureFor(const std::string& name, std::vector<unsigned char>& out)
{
	std::map<std::string, Entry>::iterator it = g_skins.find(name);
	if (it == g_skins.end()) return false;
	out.assign(it->second.rgba.begin(), it->second.rgba.begin() + 64 * 32 * 4);
	if (it->second.slim) {
		// 3px Alex arms on the 4px classic arm: widen by repeating the outer column
		unsigned char* img = &out[0];
		for (int y = 16; y < 32; ++y) {
			unsigned char row[64 * 4];
			memcpy(row, img + y * 64 * 4, sizeof(row));
			// slim arm faces: outer 40-43, front 44-46, inner 47-50, back 51-53
			// (top 44-46, bottom 47-49); classic: 4px each from 40 (top 44, bottom 48)
			static const int sides[16] = { 40,41,42,43, 44,45,46,46, 47,48,49,50, 51,52,53,53 };
			static const int caps[16]  = { 40,41,42,43, 44,45,46,46, 47,48,49,49, 52,53,54,55 };
			const int* src = y < 20 ? caps : sides;
			for (int x = 0; x < 16; ++x) memcpy(img + (y * 64 + 40 + x) * 4, row + src[x] * 4, 4);
		}
	}
	return true;
}

void PlayerSkin::remove(Minecraft* mc, const std::string& name)
{
	g_skins.erase(name);
	if (mc && mc->textures) mc->textures->releaseTexture(name);
}

void PlayerSkin::reload(Minecraft* mc)
{
	std::vector<unsigned char> img;
	g_localCustom = false;
	int choice = mc->options.skin;
	bool ok = false;
	if (choice == 2) {
		ok = loadImage64(mc, customSkinPath(mc), false, img);
		g_localCustom = ok;
		g_localSlim = false;
	}
	if (!ok && choice == 1) { ok = loadImage64(mc, "mob/alex.png", true, img); g_localSlim = ok; }
	if (!ok) { ok = loadImage64(mc, "mob/char.png", true, img); g_localSlim = false; }
	if (!ok) return;
	set(mc, LOCAL, &img[0], g_localSlim);

	g_localEagler.resize(64 * 64 * 4);
	for (int i = 0; i < 64 * 64; ++i) {
		const unsigned char* p = &img[i * 4];
		unsigned char* q = &g_localEagler[i * 4];
		q[0] = p[3]; q[1] = p[2]; q[2] = p[1]; q[3] = p[0];   // RGBA -> ABGR
	}
}

const std::vector<unsigned char>& PlayerSkin::localEaglerPixels() { return g_localEagler; }
bool PlayerSkin::localIsSlim() { return g_localSlim; }
bool PlayerSkin::localIsCustom() { return g_localCustom; }
