#include "MouseHandler.h"
#include "player/input/ITurnInput.h"

#ifdef RPI
#include <SDL/SDL.h>
#endif

#ifdef PLATFORM_GLFW
#include <GLFW/glfw3.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
bool webIsTouchDevice(); // platform/WebGLBind.cpp
#endif

MouseHandler::MouseHandler( ITurnInput* turnInput )
:	_turnInput(turnInput)
{}

MouseHandler::MouseHandler()
:	_turnInput(0)
{}

MouseHandler::~MouseHandler() {
}

void MouseHandler::setTurnInput( ITurnInput* turnInput ) {
	_turnInput = turnInput;
}

void MouseHandler::grab() {
	xd = 0;
	yd = 0;

#if defined(RPI)
	//LOGI("Grabbing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_ShowCursor(0);
#endif

#ifdef __EMSCRIPTEN__
	// Browsers only grant pointer lock inside a user gesture: defer the
	// request to the next click / key press (touch devices don't need it).
	if (webIsTouchDevice()) return;
	emscripten_request_pointerlock("#canvas", 1);
#endif
#ifdef PLATFORM_GLFW
	glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#endif
}

void MouseHandler::release() {
#if defined(RPI)
	//LOGI("Releasing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(1);
#endif

#ifdef __EMSCRIPTEN__
	if (!webIsTouchDevice()) emscripten_exit_pointerlock();
#endif
#ifdef PLATFORM_GLFW
	glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif
}

void MouseHandler::poll() {
	if (_turnInput != 0) {
		TurnDelta td = _turnInput->getTurnDelta();
		xd = td.x;
		yd = td.y;
	}
}
