#ifndef NET_MINECRAFT_WORLD_LEVEL__WorldBorder_H__
#define NET_MINECRAFT_WORLD_LEVEL__WorldBorder_H__

#include <vector>
#include "../phys/AABB.h"

class CompoundTag;
class Entity;

/**
 * Java Edition style world border.
 *
 * A square, axis aligned border centred on (centerX, centerZ) with a side
 * length of getSize(). The size can be interpolated over time (shrinking /
 * growing borders). Entities inside the border collide with it, entities
 * outside take damage, and the renderer draws the animated force field when
 * the camera comes close.
 *
 * All positional math is done in double precision so that the border edges
 * stay exact even for large coordinates.
 */
class WorldBorder
{
public:
	enum State { STATIONARY = 0, GROWING = 1, SHRINKING = 2 };

	static const double MAX_SIZE;

	WorldBorder();

	// ---- geometry ----------------------------------------------------
	double getCenterX() const { return centerX; }
	double getCenterZ() const { return centerZ; }
	void   setCenter(double x, double z);

	/** Current diameter (side length) in blocks, interpolated if lerping. */
	double getSize() const;
	double getLerpTarget() const { return lerpTo; }
	long long getLerpRemainingTime() const;

	void setSize(double size);
	void lerpSizeBetween(double from, double to, long long durationMs);

	double getMinX() const;
	double getMaxX() const;
	double getMinZ() const;
	double getMaxZ() const;

	State getState() const;

	bool isWithinBounds(double x, double z) const;
	bool isWithinBounds(const AABB& box) const;
	bool isWithinBounds(int tileX, int tileZ) const;

	/** Distance from (x, z) to the nearest border edge; negative = outside. */
	double getDistanceToBorder(double x, double z) const;

	/** Appends wall collision boxes overlapping `query` (for entities inside). */
	void addCollisionBoxes(const AABB& query, std::vector<AABB>& out) const;

	// ---- gameplay ----------------------------------------------------
	double getDamagePerBlock() const   { return damagePerBlock; }
	void   setDamagePerBlock(double d) { damagePerBlock = d; }
	double getDamageSafeZone() const   { return damageSafeZone; }
	void   setDamageSafeZone(double d) { damageSafeZone = d; }
	int    getWarningBlocks() const    { return warningBlocks; }
	void   setWarningBlocks(int b)     { warningBlocks = b; }
	int    getWarningTime() const      { return warningTime; }
	void   setWarningTime(int s)       { warningTime = s; }

	/** Damage an entity at (x, z) should take this tick (0 = none). */
	int getDamageFor(double x, double z) const;

	/** 0..1 strength of the red "too close to the border" screen tint. */
	float getWarningStrength(double x, double z) const;

	/** Moves the interpolation forward; called once per level tick. */
	void tick();

	// ---- persistence -------------------------------------------------
	void save(CompoundTag* tag) const;
	void load(const CompoundTag* tag);

	/** Fit the border to the whole (finite) level. */
	void resetToLevel(int levelWidth, int levelDepth);

private:
	static long long nowMs();

	double centerX, centerZ;
	double lerpFrom, lerpTo;
	long long lerpStart, lerpEnd;

	double damagePerBlock;
	double damageSafeZone;
	int warningBlocks;
	int warningTime;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__WorldBorder_H__*/
