#include "WorldBorder.h"
#include "../../nbt/CompoundTag.h"
#include <chrono>
#include <cmath>
#include <algorithm>

const double WorldBorder::MAX_SIZE = 60000000.0;

static double clampd(double v, double lo, double hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

long long WorldBorder::nowMs() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

WorldBorder::WorldBorder()
:	centerX(0), centerZ(0),
	lerpFrom(MAX_SIZE), lerpTo(MAX_SIZE),
	lerpStart(0), lerpEnd(0),
	damagePerBlock(0.2),
	damageSafeZone(5.0),
	warningBlocks(5),
	warningTime(15)
{
}

void WorldBorder::setCenter(double x, double z) {
	centerX = x;
	centerZ = z;
}

double WorldBorder::getSize() const {
	if (lerpEnd <= lerpStart) return lerpTo;
	const long long now = nowMs();
	if (now >= lerpEnd) return lerpTo;
	const double t = (double)(now - lerpStart) / (double)(lerpEnd - lerpStart);
	return lerpFrom + (lerpTo - lerpFrom) * clampd(t, 0.0, 1.0);
}

long long WorldBorder::getLerpRemainingTime() const {
	const long long rem = lerpEnd - nowMs();
	return rem > 0 ? rem : 0;
}

void WorldBorder::setSize(double size) {
	size = clampd(size, 1.0, MAX_SIZE);
	lerpFrom = lerpTo = size;
	lerpStart = lerpEnd = nowMs();
}

void WorldBorder::lerpSizeBetween(double from, double to, long long durationMs) {
	from = clampd(from, 1.0, MAX_SIZE);
	to   = clampd(to,   1.0, MAX_SIZE);
	if (durationMs <= 0 || from == to) {
		setSize(to);
		return;
	}
	lerpFrom  = from;
	lerpTo    = to;
	lerpStart = nowMs();
	lerpEnd   = lerpStart + durationMs;
}

double WorldBorder::getMinX() const { return centerX - getSize() * 0.5; }
double WorldBorder::getMaxX() const { return centerX + getSize() * 0.5; }
double WorldBorder::getMinZ() const { return centerZ - getSize() * 0.5; }
double WorldBorder::getMaxZ() const { return centerZ + getSize() * 0.5; }

WorldBorder::State WorldBorder::getState() const {
	if (getLerpRemainingTime() <= 0 || lerpFrom == lerpTo) return STATIONARY;
	return lerpTo < lerpFrom ? SHRINKING : GROWING;
}

bool WorldBorder::isWithinBounds(double x, double z) const {
	const double h = getSize() * 0.5;
	return x > centerX - h && x < centerX + h && z > centerZ - h && z < centerZ + h;
}

bool WorldBorder::isWithinBounds(int tileX, int tileZ) const {
	// a tile is inside when its whole column is inside the border
	const double h = getSize() * 0.5;
	return (double)tileX + 1 > centerX - h && (double)tileX < centerX + h
	    && (double)tileZ + 1 > centerZ - h && (double)tileZ < centerZ + h;
}

bool WorldBorder::isWithinBounds(const AABB& box) const {
	const double h = getSize() * 0.5;
	return box.x1 > centerX - h && box.x0 < centerX + h
	    && box.z1 > centerZ - h && box.z0 < centerZ + h;
}

double WorldBorder::getDistanceToBorder(double x, double z) const {
	const double h = getSize() * 0.5;
	const double dMinX = x - (centerX - h);
	const double dMaxX = (centerX + h) - x;
	const double dMinZ = z - (centerZ - h);
	const double dMaxZ = (centerZ + h) - z;
	return std::min(std::min(dMinX, dMaxX), std::min(dMinZ, dMaxZ));
}

void WorldBorder::addCollisionBoxes(const AABB& q, std::vector<AABB>& out) const {
	const double minX = getMinX(), maxX = getMaxX();
	const double minZ = getMinZ(), maxZ = getMaxZ();

	// Only bother when the query box actually touches a border plane
	const double reach = 2.0;
	const bool nearX0 = q.x0 < minX + reach && q.x1 > minX - reach;
	const bool nearX1 = q.x0 < maxX + reach && q.x1 > maxX - reach;
	const bool nearZ0 = q.z0 < minZ + reach && q.z1 > minZ - reach;
	const bool nearZ1 = q.z0 < maxZ + reach && q.z1 > maxZ - reach;
	if (!(nearX0 || nearX1 || nearZ0 || nearZ1)) return;

	const float y0 = q.y0 - 1.0f, y1 = q.y1 + 1.0f;
	const float zLo = (float)std::max((double)q.z0 - 1.0, minZ - 1.0);
	const float zHi = (float)std::min((double)q.z1 + 1.0, maxZ + 1.0);
	const float xLo = (float)std::max((double)q.x0 - 1.0, minX - 1.0);
	const float xHi = (float)std::min((double)q.x1 + 1.0, maxX + 1.0);

	if (nearX0) out.push_back(AABB((float)(minX - 1.0), y0, zLo, (float)minX, y1, zHi));
	if (nearX1) out.push_back(AABB((float)maxX, y0, zLo, (float)(maxX + 1.0), y1, zHi));
	if (nearZ0) out.push_back(AABB(xLo, y0, (float)(minZ - 1.0), xHi, y1, (float)minZ));
	if (nearZ1) out.push_back(AABB(xLo, y0, (float)maxZ, xHi, y1, (float)(maxZ + 1.0)));
}

int WorldBorder::getDamageFor(double x, double z) const {
	const double d = getDistanceToBorder(x, z) + damageSafeZone;
	if (d >= 0.0 || damagePerBlock <= 0.0) return 0;
	return std::max(1, (int)std::floor(-d * damagePerBlock));
}

float WorldBorder::getWarningStrength(double x, double z) const {
	const double dist = getDistanceToBorder(x, z);
	double warn = std::max((double)warningBlocks, 1.0);
	// Java: a moving border warns earlier, based on where it will be in warningTime seconds
	if (getState() != STATIONARY && lerpEnd > lerpStart) {
		const double blocksPerSecond = std::fabs(lerpTo - lerpFrom) / (double)(lerpEnd - lerpStart) * 1000.0;
		warn = std::max(warn, std::min(blocksPerSecond * warningTime, std::fabs(lerpTo - getSize())));
	}
	if (dist >= warn) return 0.0f;
	if (dist <= 0.0) return 1.0f;
	return (float)(1.0 - dist / warn);
}

void WorldBorder::tick() {
	// getSize() is time based; collapse finished lerps so the state settles.
	if (lerpEnd > lerpStart && nowMs() >= lerpEnd) {
		lerpFrom = lerpTo;
		lerpStart = lerpEnd;
	}
}

void WorldBorder::save(CompoundTag* tag) const {
	if (!tag) return;
	tag->putDouble("BorderCenterX", centerX);
	tag->putDouble("BorderCenterZ", centerZ);
	tag->putDouble("BorderSize", getSize());
	tag->putLong("BorderSizeLerpTime", getLerpRemainingTime());
	tag->putDouble("BorderSizeLerpTarget", lerpTo);
	tag->putDouble("BorderDamagePerBlock", damagePerBlock);
	tag->putDouble("BorderSafeZone", damageSafeZone);
	tag->putDouble("BorderWarningBlocks", (double)warningBlocks);
	tag->putDouble("BorderWarningTime", (double)warningTime);
}

void WorldBorder::load(const CompoundTag* tag) {
	if (!tag || !tag->contains("BorderSize")) return;
	centerX = tag->getDouble("BorderCenterX");
	centerZ = tag->getDouble("BorderCenterZ");
	const double size = tag->getDouble("BorderSize");
	const long long lerpTime = tag->getLong("BorderSizeLerpTime");
	if (lerpTime > 0)
		lerpSizeBetween(size, tag->getDouble("BorderSizeLerpTarget"), lerpTime);
	else
		setSize(size);
	if (tag->contains("BorderDamagePerBlock")) damagePerBlock = tag->getDouble("BorderDamagePerBlock");
	if (tag->contains("BorderSafeZone"))       damageSafeZone = tag->getDouble("BorderSafeZone");
	if (tag->contains("BorderWarningBlocks"))  warningBlocks  = (int)tag->getDouble("BorderWarningBlocks");
	if (tag->contains("BorderWarningTime"))    warningTime    = (int)tag->getDouble("BorderWarningTime");
}

void WorldBorder::resetToLevel(int levelWidth, int levelDepth) {
	setCenter(levelWidth * 0.5, levelDepth * 0.5);
	setSize((double)std::max(levelWidth, levelDepth));
}
