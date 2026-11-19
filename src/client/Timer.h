#ifndef NET_MINECRAFT_CLIENT__Timer_H__
#define NET_MINECRAFT_CLIENT__Timer_H__

//package net.minecraft.client;
#include "../platform/time.h"

/**
 * Fixed rate tick timer.
 *
 * Uses the monotonic double precision clock (getTimeSD) so frame deltas stay
 * exact no matter how long the game has been running; the original float
 * millisecond based version lost precision after a few hours of uptime,
 * making movement interpolation visibly jerky.
 */
class Timer
{
public:
	Timer(float ticksPerSecond)
	:	ticksPerSecond(ticksPerSecond),
		ticks(0),
		a(0),
		timeScale(1.0f),
		passedTime(0),
		lastTime(getTimeSD())
	{
	}

	void advanceTime() {
		const double now = getTimeSD();
		double passedSeconds = now - lastTime;
		lastTime = now;

		if (passedSeconds < 0) passedSeconds = 0;
		if (passedSeconds > 1) passedSeconds = 1;

		passedTime += passedSeconds * timeScale * ticksPerSecond;

		ticks = (int) passedTime;
		passedTime -= ticks;
		if (ticks > MAX_TICKS_PER_UPDATE) ticks = MAX_TICKS_PER_UPDATE;
		a = (float) passedTime;
	}

	/**
	 * Advances time the max number of ticks per second.
	 */
	void advanceTimeQuickly() {
		const double passedSeconds = (double) MAX_TICKS_PER_UPDATE / (double) ticksPerSecond;

		passedTime += passedSeconds * timeScale * ticksPerSecond;
		ticks = (int) passedTime;
		passedTime -= ticks;
		a = (float) passedTime;

		lastTime = getTimeSD();
	}

	/** Swallows the elapsed time without producing ticks (e.g. after loading). */
	void skipTime() {
		lastTime = getTimeSD();
		ticks = 0;
	}

public:
	float ticksPerSecond;
	int ticks;
	float a;
	float timeScale;
	double passedTime;
private:
	static const int MAX_TICKS_PER_UPDATE = 10;

	double lastTime;
};

#endif /*NET_MINECRAFT_CLIENT__Timer_H__*/
