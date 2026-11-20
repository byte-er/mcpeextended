#ifndef NET_MINECRAFT_CLIENT_SKIN__PlayerSkin_H__
#define NET_MINECRAFT_CLIENT_SKIN__PlayerSkin_H__

#include <string>
#include <vector>

class Minecraft;

/**
 * Player skins.
 *
 * Skins are kept as 64x64 RGBA images (the 1.8 layout; old 64x32 skins are
 * expanded). The player model in this engine uses the classic 64x32 layout,
 * so the texture handed to the renderer is the top half. Textures are
 * registered under names starting with "skin:" and Textures rebuilds them on
 * demand (also after the GL context is lost).
 *
 * Local skin (Options -> skin): 0 Steve, 1 Alex, 2 custom file
 * games/com.mojang/skin.png in the game's storage folder (the browser build
 * lets you pick the file).
 */
namespace PlayerSkin
{
	static const char* const LOCAL = "skin:local";

	/** Re-reads the chosen local skin (call after changing Options::skin). */
	void reload(Minecraft* mc);

	/** Path of the custom skin file. */
	std::string customSkinPath(Minecraft* mc);

	/** Stores a 64x64 RGBA skin under `name` ("skin:..."), replacing any old texture. */
	void set(Minecraft* mc, const std::string& name, const unsigned char* rgba64x64, bool slim);
	/** True if `name` is registered; `out` gets the 64x32 RGBA texture for the model. */
	bool textureFor(const std::string& name, std::vector<unsigned char>& out64x32);
	void remove(Minecraft* mc, const std::string& name);

	/** Local skin as Eaglercraft profile data: 64x64 [A,B,G,R] pixels. */
	const std::vector<unsigned char>& localEaglerPixels();
	bool localIsSlim();
	bool localIsCustom();

	/** Loads a PNG (asset name or file path) as 64x64 RGBA. */
	bool loadImage64(Minecraft* mc, const std::string& name, bool asset, std::vector<unsigned char>& out);
}

#endif /*NET_MINECRAFT_CLIENT_SKIN__PlayerSkin_H__*/
