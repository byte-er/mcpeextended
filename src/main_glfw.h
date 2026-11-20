#ifndef MAIN_GLFW_H__
#define MAIN_GLFW_H__

#include "App.h"
#include "GLFW/glfw3.h"
#include "client/renderer/gles.h"
#include "SharedConstants.h"

#include <cstdio>
#include <chrono>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <thread>
#include "platform/input/Keyboard.h"
#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include "world/level/LevelSettings.h"
#include "client/gui/screens/ProgressScreen.h"
#include <cstdlib>

static App* g_app = 0;

int transformKey(int glfwkey) {
	if (glfwkey >= GLFW_KEY_F1 && glfwkey <= GLFW_KEY_F12) {
		return glfwkey - 178;
	}

	switch (glfwkey) {
		case GLFW_KEY_ESCAPE: return Keyboard::KEY_ESCAPE;
		case GLFW_KEY_BACKSPACE: return Keyboard::KEY_BACKSPACE;
		case GLFW_KEY_LEFT_SHIFT: return Keyboard::KEY_LSHIFT;
		case GLFW_KEY_ENTER: return Keyboard::KEY_RETURN;
		default: return glfwkey;
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if(action == GLFW_REPEAT) return;

	if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
		GLFWmonitor* monitor = glfwGetWindowMonitor(window);
		if (monitor) {
			// Currently fullscreen → go windowed
			glfwSetWindowMonitor(window, NULL, 80, 80, 854, 480, 0);
		} else {
			// Currently windowed → go fullscreen on primary monitor
			GLFWmonitor* primary = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(primary);
			glfwSetWindowMonitor(window, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		return;
	}

	Keyboard::feed(transformKey(key), action);
}

void character_callback(GLFWwindow* window, unsigned int codepoint) {
	Keyboard::feedText(codepoint);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	static double lastX = 0.0, lastY = 0.0;
	static bool firstMouse = true;

	if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

	double deltaX = xpos - lastX;
    double deltaY = ypos - lastY;

    lastX = xpos;
    lastY = ypos;

	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
		Mouse::feed(0, 0, xpos, ypos, deltaX, deltaY);
	} else { 
		Mouse::feed( MouseAction::ACTION_MOVE, 0, xpos, ypos);
	}
	Multitouch::feed(0, 0, xpos, ypos, 0);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(action == GLFW_REPEAT) return;

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		Mouse::feed( MouseAction::ACTION_LEFT, action, xpos, ypos);
		Multitouch::feed(1, action, xpos, ypos, 0);
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		Mouse::feed( MouseAction::ACTION_RIGHT, action, xpos, ypos);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	Mouse::feed(3, 0, xpos, ypos, 0, yoffset);
}

void window_size_callback(GLFWwindow* window, int width, int height) {
	if (g_app) g_app->setSize(width, height);
}

void error_callback(int error, const char* desc) {
	printf("Error: %s\n", desc);
}

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------------------
// Web (HTML5 / PWA) glue: touch input, canvas resize / rotation, main loop
// ---------------------------------------------------------------------------
static GLFWwindow* g_window = 0;

static double webDpr() { return emscripten_get_device_pixel_ratio(); }

static EM_BOOL web_touch(int type, const EmscriptenTouchEvent* e, void*) {
	const double dpr = webDpr();
	for (int i = 0; i < e->numTouches; ++i) {
		const EmscriptenTouchPoint& t = e->touches[i];
		if (!t.isChanged) continue;
		const int x = (int)(t.targetX * dpr), y = (int)(t.targetY * dpr);
		const int id = (int)(t.identifier % 16);
		if (type == EMSCRIPTEN_EVENT_TOUCHSTART) {
			Mouse::feed(1, 1, x, y);
			Multitouch::feed(1, 1, x, y, id);
		} else if (type == EMSCRIPTEN_EVENT_TOUCHMOVE) {
			Mouse::feed(0, 0, x, y);
			Multitouch::feed(0, 0, x, y, id);
		} else {
			Mouse::feed(1, 0, x, y);
			Multitouch::feed(1, 0, x, y, id);
		}
	}
	return EM_TRUE; // no emulated mouse events / scrolling
}

static void webResize() {
	double cssW = 0, cssH = 0;
	emscripten_get_element_css_size("#canvas", &cssW, &cssH);
	const double dpr = webDpr();
	const int w = (int)(cssW * dpr), h = (int)(cssH * dpr);
	if (w <= 0 || h <= 0) return;
	emscripten_set_canvas_element_size("#canvas", w, h);
	if (g_window) glfwSetWindowSize(g_window, w, h);
	if (g_app) g_app->setSize(w, h);
}

static EM_BOOL web_resize(int, const EmscriptenUiEvent*, void*) { webResize(); return EM_FALSE; }

// The browser releases pointer lock on Esc: open the pause menu, as Java does.
// Only react to a real locked -> unlocked transition: browsers also send
// "not locked" change events when a lock request fails or is pending, and
// pausing on those made "Back to game" bounce straight back to the menu.
static bool g_webPointerLocked = false;
// Custom skin picked in the browser (see web/shell.html mcpePickSkin)
extern "C" EMSCRIPTEN_KEEPALIVE void web_skin_changed() {
	if (g_app) ((MAIN_CLASS*)g_app)->platform()->notifySkinChanged();
}

// Soft keyboard bridge for touch browsers (see web/shell.html)
extern "C" EMSCRIPTEN_KEEPALIVE void web_kbd_char(int c) {
	if (c >= 32 && c < 127) Keyboard::feedText((char)c);
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_kbd_key(int k) {
	int key = k == 8 ? Keyboard::KEY_BACKSPACE : k == 13 ? Keyboard::KEY_RETURN : k == 27 ? Keyboard::KEY_ESCAPE : -1;
	if (key < 0) return;
	Keyboard::feed((unsigned char)key, 1);
	Keyboard::feed((unsigned char)key, 0);
}

static EM_BOOL web_pointerlock(int, const EmscriptenPointerlockChangeEvent* e, void*) {
	MAIN_CLASS* mc = (MAIN_CLASS*)g_app;
	const bool wasLocked = g_webPointerLocked;
	g_webPointerLocked = e->isActive != 0;
	if (wasLocked && !e->isActive && mc && mc->mouseGrabbed && mc->level && !mc->screen)
		mc->pauseGame(false);
	return EM_FALSE;
}
static EM_BOOL web_orientation(int, const EmscriptenOrientationChangeEvent*, void*) { webResize(); return EM_FALSE; }

static void web_main_loop() {
	App* app = g_app;
	app->update();
	glfwPollEvents();
	if (app->wantToQuit()) emscripten_cancel_main_loop();
}
#endif

int main(void) {
	AppContext appContext;

#ifndef STANDALONE_SERVER
	// Platform init.
	appContext.platform = new AppPlatform_glfw();

	glfwSetErrorCallback(error_callback);

	if (!glfwInit()) {
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	GLFWwindow* window = glfwCreateWindow(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight(), "main", NULL, NULL);
	
	if (window == NULL) {
		return 1;
	}

	glfwSetKeyCallback(window, key_callback);
	glfwSetCharCallback(window, character_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);

	glfwMakeContextCurrent(window);
#ifdef __EMSCRIPTEN__
	g_window = window;
	emscripten_set_touchstart_callback("#canvas", 0, 1, web_touch);
	emscripten_set_touchmove_callback("#canvas", 0, 1, web_touch);
	emscripten_set_touchend_callback("#canvas", 0, 1, web_touch);
	emscripten_set_touchcancel_callback("#canvas", 0, 1, web_touch);
	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, web_resize);
	emscripten_set_orientationchange_callback(0, 1, web_orientation);
	emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, web_pointerlock);
#endif
	gladLoadGLES1Loader((GLADloadproc)winGLLoader);
#ifdef __EMSCRIPTEN__
	{ extern void webBindGLFunctions(); webBindGLFunctions(); }
#endif
	glfwSwapInterval(0);
	glPatchDesktopCompat();
#endif

	App* app = new MAIN_CLASS();

	g_app = app;
#ifdef __EMSCRIPTEN__
	// IndexedDB backed folder, mounted and synced by web/shell.html
	((MAIN_CLASS*)g_app)->externalStoragePath = "/persist";
	((MAIN_CLASS*)g_app)->externalCacheStoragePath = "/persist";
#else
	((MAIN_CLASS*)g_app)->externalStoragePath = ".";
	((MAIN_CLASS*)g_app)->externalCacheStoragePath = ".";
#endif
	g_app->init(appContext);
	g_app->setSize(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());

	// Developer convenience: MCPE_AUTOWORLD=<name> [MCPE_SEED=<n>] [MCPE_CREATIVE=1]
	// loads (or creates) a world straight away, skipping the menus.
	if (getenv("MCPE_ANAGLYPH")) {
		MAIN_CLASS* mc = (MAIN_CLASS*)g_app;
		mc->options.anaglyph3d = true;
		mc->options.onAnaglyphChanged();
	}
	if (const char* eaglerAddr = getenv("MCPE_EAGLER")) {
		MAIN_CLASS* mc = (MAIN_CLASS*)g_app;
		if (const char* name = getenv("MCPE_USER")) mc->options.username = name;
		mc->joinEaglerServer(eaglerAddr);
	}
	if (const char* autoWorld = getenv("MCPE_AUTOWORLD")) {
		MAIN_CLASS* mc = (MAIN_CLASS*)g_app;
		const char* seedStr = getenv("MCPE_SEED");
		const char* creative = getenv("MCPE_CREATIVE");
		const char* worldType = getenv("MCPE_WORLDTYPE"); // 0 large, 1 old, 2 flat
		LevelSettings settings(seedStr ? atol(seedStr) : 12345L,
			(creative && creative[0] == '1') ? GameType::Creative : GameType::Survival,
			worldType ? atoi(worldType) : LevelSettings::WORLD_LARGE);
		mc->selectLevel(autoWorld, autoWorld, settings);
		mc->hostMultiplayer();
		mc->setScreen(new ProgressScreen());
	}

#ifdef __EMSCRIPTEN__
	webResize();
	emscripten_set_main_loop(web_main_loop, 0, 1); // never returns
	return 0;
#endif

	// Main event loop
	using clock = std::chrono::steady_clock;
	while(!glfwWindowShouldClose(window) && !app->wantToQuit()) {
		auto frameStart = clock::now();

		app->update();

		glfwSwapBuffers(window);
		glfwPollEvents();

		glfwSwapInterval(((MAIN_CLASS*)app)->options.vsync ? 1 : 0);
		if(((MAIN_CLASS*)app)->options.limitFramerate) {
			auto frameEnd = clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);
			auto target = std::chrono::microseconds(33333); // ~30 fps
			if(elapsed < target)
				std::this_thread::sleep_for(target - elapsed);
		}
	}

	delete app;

	appContext.platform->finish();
	
	delete appContext.platform;
	
#ifndef STANDALONE_SERVER
	// Exit.
	glfwDestroyWindow(window);
	glfwTerminate();
#endif

	return 0;
}

#endif /*MAIN_GLFW_H__*/
