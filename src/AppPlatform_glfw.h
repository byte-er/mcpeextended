#ifndef APPPLATFORM_GLFW_H__
#define APPPLATFORM_GLFW_H__

#include <ctime>
#include "AppPlatform.h"
#include "platform/log.h"
#include "client/renderer/gles.h"
#include "world/level/storage/FolderMethods.h"
#include <png.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <deque>
#endif
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static void png_funcReadFile(png_structp pngPtr, png_bytep data, png_size_t length) {
	((std::istream*)png_get_io_ptr(pngPtr))->read((char*)data, length);
}

class AppPlatform_glfw: public AppPlatform
{
public:
    AppPlatform_glfw()
#ifdef __EMSCRIPTEN__
    :   _sock(NULL)
#endif
    {
    }

#ifdef __EMSCRIPTEN__
	// ---- WebSocket via the browser (Eaglercraft servers) ----
	struct WebSock {
		EMSCRIPTEN_WEBSOCKET_T ws;
		int state;
		std::string error;
		std::deque<std::vector<unsigned char> > inbox;
		WebSock() : ws(0), state(WS_CONNECTING) {}
	};
	WebSock* _sock;

	static EM_BOOL wsOnOpen(int, const EmscriptenWebSocketOpenEvent*, void* u) {
		((WebSock*)u)->state = WS_OPEN; return EM_TRUE;
	}
	static EM_BOOL wsOnMessage(int, const EmscriptenWebSocketMessageEvent* e, void* u) {
		if (!e->isText && e->numBytes > 0)
			((WebSock*)u)->inbox.push_back(std::vector<unsigned char>(e->data, e->data + e->numBytes));
		return EM_TRUE;
	}
	static EM_BOOL wsOnError(int, const EmscriptenWebSocketErrorEvent*, void* u) {
		WebSock* s = (WebSock*)u;
		if (s->error.empty()) s->error = "Could not connect to the server";
		return EM_TRUE;
	}
	static EM_BOOL wsOnClose(int, const EmscriptenWebSocketCloseEvent* e, void* u) {
		WebSock* s = (WebSock*)u;
		s->state = WS_CLOSED;
		LOGI("WebSocket closed: code %d clean %d reason '%s'\n", (int)e->code, (int)e->wasClean, e->reason);
		if (s->error.empty() && e->reason[0]) s->error = e->reason;
		return EM_TRUE;
	}

	virtual bool wsSupported() { return emscripten_websocket_is_supported() != 0; }
	virtual int wsOpen(const std::string& url) {
		wsClose(0);
		_sock = new WebSock();
		EmscriptenWebSocketCreateAttributes attr;
		emscripten_websocket_init_create_attributes(&attr);
		attr.url = url.c_str();
		attr.createOnMainThread = EM_TRUE;
		EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&attr);
		if (ws <= 0) { _sock->state = WS_CLOSED; _sock->error = "Invalid server address"; return 0; }
		_sock->ws = ws;
		emscripten_websocket_set_onopen_callback(ws, _sock, wsOnOpen);
		emscripten_websocket_set_onmessage_callback(ws, _sock, wsOnMessage);
		emscripten_websocket_set_onerror_callback(ws, _sock, wsOnError);
		emscripten_websocket_set_onclose_callback(ws, _sock, wsOnClose);
		return 0;
	}
	virtual void wsSend(int, const unsigned char* data, int len) {
		if (_sock && _sock->state == WS_OPEN)
			emscripten_websocket_send_binary(_sock->ws, (void*)data, len);
	}
	virtual bool wsPoll(int, std::vector<unsigned char>& out) {
		if (!_sock || _sock->inbox.empty()) return false;
		out.swap(_sock->inbox.front());
		_sock->inbox.pop_front();
		return true;
	}
	virtual int wsState(int) { return _sock ? _sock->state : WS_CLOSED; }
	virtual std::string wsError(int) { return _sock ? _sock->error : std::string(); }
	virtual void wsClose(int) {
		if (!_sock) return;
		if (_sock->ws > 0) {
			emscripten_websocket_close(_sock->ws, 1000, "");
			emscripten_websocket_delete(_sock->ws);
		}
		delete _sock;
		_sock = NULL;
	}

	virtual bool pickSkinFile(const std::string& destPath) {
		EM_ASM({ if (window.mcpePickSkin) window.mcpePickSkin(UTF8ToString($0)); }, destPath.c_str());
		return true;
	}

	// Touch browsers have no physical keyboard: web/shell.html focuses a
	// hidden <input> (or shows a "tap to type" bar on iOS) and feeds the
	// typed text back through web_kbd_char / web_kbd_key.
	virtual void showKeyboard() {
		AppPlatform::showKeyboard();
		EM_ASM({ if (window.mcpeKeyboard) window.mcpeKeyboard(true); });
	}
	virtual void hideKeyboard() {
		AppPlatform::hideKeyboard();
		EM_ASM({ if (window.mcpeKeyboard) window.mcpeKeyboard(false); });
	}
#endif

	BinaryBlob readAssetFile(const std::string& filename) {
		FILE* fp = fopen(("data/" + filename).c_str(), "r");
		if (!fp)
			return BinaryBlob();

		int size = getRemainingFileSize(fp);

		BinaryBlob blob;
		blob.size = size;
		blob.data = new unsigned char[size];

		fread(blob.data, 1, size, fp);
		fclose(fp);

		return blob;
	}

    void saveScreenshot(const std::string& filename, int glWidth, int glHeight) {
        //@todo
    }

    __inline unsigned int rgbToBgr(unsigned int p) {
        return (p & 0xff00ff00) | ((p >> 16) & 0xff) | ((p << 16) & 0xff0000);
    }

    TextureData loadTexture(const std::string& filename_, bool textureFolder)
	{
		TextureData out;

		std::string filename = textureFolder? "data/images/" + filename_
											: filename_;
		std::ifstream source(filename.c_str(), std::ios::binary);

		if (source) {
			png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

			if (!pngPtr)
				return out;

			png_infop infoPtr = png_create_info_struct(pngPtr);

			if (!infoPtr) {
				png_destroy_read_struct(&pngPtr, NULL, NULL);
				return out;
			}

			// Hack to get around the broken libpng for windows
			png_set_read_fn(pngPtr,(void*)&source, png_funcReadFile);

			png_read_info(pngPtr, infoPtr);

			// Normalise every PNG flavour (palette, grey, RGB, 16 bit, tRNS)
			// to 8 bit RGBA; the code below assumes 4 bytes per pixel.
			{
				const int colorType = png_get_color_type(pngPtr, infoPtr);
				const int bitDepth  = png_get_bit_depth(pngPtr, infoPtr);
				if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(pngPtr);
				if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(pngPtr);
				if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(pngPtr);
				if (bitDepth == 16) png_set_strip_16(pngPtr);
				if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(pngPtr);
				if (!(colorType & PNG_COLOR_MASK_ALPHA) && !png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS))
					png_set_filler(pngPtr, 0xff, PNG_FILLER_AFTER);
				png_read_update_info(pngPtr, infoPtr);
			}

			// Set up the texdata properties
			out.w = png_get_image_width(pngPtr, infoPtr);
			out.h = png_get_image_height(pngPtr, infoPtr);

			png_bytep* rowPtrs = new png_bytep[out.h];
			out.data = new unsigned char[4 * out.w * out.h];
			out.memoryHandledExternally = false;

			int rowStrideBytes = 4 * out.w;
			for (int i = 0; i < out.h; i++) {
				rowPtrs[i] = (png_bytep)&out.data[i*rowStrideBytes];
			}
			png_read_image(pngPtr, rowPtrs);

			// Teardown and return
			png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);
			delete[] (png_bytep)rowPtrs;
			source.close();

			return out;
		}
		else
		{
			LOGI("Couldn't find file: %s\n", filename.c_str());
			return out;
		}
    }

    std::string getDateString(int s) {
		// human readable local date instead of raw epoch seconds
		time_t t = (time_t)s;
		struct tm* lt = localtime(&t);
		char buf[64];
		if (lt && strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", lt) > 0)
			return std::string(buf);
        std::stringstream ss;
		ss << s << " s (UTC)";
		return ss.str();
	}

	virtual int checkLicense() {
		static int _z = 0;//20;
		_z--;
		if (_z < 0) return 0;
		//if (_z < 0) return 107;
		return -2;
	}

	virtual int getScreenWidth() { return 854; };
	virtual int getScreenHeight() { return 480; };

	virtual float getPixelsPerMillimeter();
#ifdef __EMSCRIPTEN__
	// phones: buzz when a block breaks (Options -> Feedback vibration)
	virtual void vibrate(int ms) { EM_ASM({ if (navigator.vibrate) navigator.vibrate($0); }, ms); }
#endif

	virtual bool supportsTouchscreen() { return true; }
	virtual bool hasBuyButtonWhenInvalidLicense() { return false; }

	virtual void openURL(const std::string& url) {
#ifdef _WIN32
		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
	}

private:
};

void* winGLLoader(const char* name);
void glPatchDesktopCompat();

#endif /*APPPLATFORM_GLFW_H__*/
