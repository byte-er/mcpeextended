#ifndef NET_MINECRAFT_CLIENT_GUI__GuiComponent_H__
#define NET_MINECRAFT_CLIENT_GUI__GuiComponent_H__

//package net.minecraft.client.gui;

#include <string>
class Font;
class Minecraft;

class GuiComponent
{
public:
	GuiComponent();
	virtual ~GuiComponent();

	void drawString(Font* font, const std::string& str, int x, int y, int color);
	void drawCenteredString(Font* font, const std::string& str, int x, int y, int color);

	void blit(int x, int y, int sx, int sy, int w, int h, int sw=0, int sh=0);
	void blit(float x, float y, int sx, int sy, float w, float h, int sw=0, int sh=0);
	/** Draws the sw x sh source rect into w x h keeping a b pixel border unstretched (256x256 sheets). */
	void blitNineSlice(float x, float y, float w, float h, int sx, int sy, int sw, int sh, int b, int texSize = 256);

protected:
    void fill(int x0, int y0, int x1, int y1, int col);
	void fill(float x0, float y0, float x1, float y1, int col);
    void fillGradient(int x0, int y0, int x1, int y1, int col1, int col2);
	void fillGradient(float x0, float y0, float x1, float y1, int col1, int col2);
	void fillHorizontalGradient(int x0, int y0, int x1, int y1, int col1, int col2);
	void fillHorizontalGradient(float x0, float y0, float x1, float y1, int col1, int col2);

	float blitOffset;
	
};

#endif /*NET_MINECRAFT_CLIENT_GUI__GuiComponent_H__*/
