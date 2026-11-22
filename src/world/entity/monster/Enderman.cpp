#include "Enderman.h"
#include "../player/Player.h"
#include "../../level/Level.h"
#include "../../level/tile/Tile.h"
#include "../../level/material/Material.h"
#include "../../phys/Vec3.h"
#include "../../../nbt/CompoundTag.h"
#include "../../../util/Mth.h"
#include <cmath>

Enderman::Enderman(Level* level)
:	super(level),
	stareTicks(0),
	teleportCooldown(0)
{
	entityRendererId = ER_ENDERMAN_RENDERER;
	textureName = "mob/enderman.png";
	setSize(0.6f, 2.9f);
	footSize = 1.0f;          // steps up full blocks
	runSpeed = 0.7f;
	attackDamage = 7;
	entityData.define(DATA_CARRY_ID, (SynchedEntityData::TypeShort)0);
	entityData.define(DATA_CARRY_DATA, (SynchedEntityData::TypeChar)0);
	entityData.define(DATA_CREEPY, (SynchedEntityData::TypeChar)0);
}

int Enderman::getMaxHealth() { return 40; }
int Enderman::getEntityTypeId() const { return MobTypes::Enderman; }

int  Enderman::getCarriedTile() const { return entityData.getShort(DATA_CARRY_ID); }
int  Enderman::getCarriedData() const { return entityData.getByte(DATA_CARRY_DATA); }
void Enderman::setCarried(int tile, int data) {
	entityData.set(DATA_CARRY_ID, (SynchedEntityData::TypeShort)tile);
	entityData.set(DATA_CARRY_DATA, (SynchedEntityData::TypeChar)data);
}
bool Enderman::isCreepy() const { return entityData.getByte(DATA_CREEPY) != 0; }
void Enderman::setCreepy(bool v) { entityData.set(DATA_CREEPY, (SynchedEntityData::TypeChar)(v ? 1 : 0)); }

bool Enderman::canCarry(int t) {
	if (t <= 0 || t >= 256 || !Tile::tiles[t]) return false;
	return t == ((Tile*)Tile::grass)->id || t == Tile::dirt->id || t == Tile::sand->id || t == Tile::gravel->id
		|| t == ((Tile*)Tile::flower)->id || t == ((Tile*)Tile::rose)->id
		|| t == ((Tile*)Tile::mushroom1)->id || t == ((Tile*)Tile::mushroom2)->id
		|| t == Tile::tnt->id || t == Tile::cactus->id || t == Tile::clay->id || t == Tile::melon->id
		|| (Tile::mycelium && t == Tile::mycelium->id) || (Tile::podzol && t == Tile::podzol->id);
}

// A player stares at us: their view ray passes close to our head and nothing is in between
bool Enderman::isLookingAtMe(Mob* p) {
	Vec3 look = p->getViewVector(1);
	look = look.normalized();
	Vec3 toMe(x - p->x, (bb.y0 + bbHeight * 0.9f) - (p->y), z - p->z);
	const float dist = toMe.length();
	if (dist < 0.001f) return false;
	toMe = toMe.normalized();
	const float dot = look.dot(toMe);
	return dot > 1.0f - 0.025f / dist && p->canSee(this);
}

Entity* Enderman::findAttackTarget() {
	Player* p = level->getNearestPlayer(this, 64);
	if (!p || p->abilities.invulnerable) { stareTicks = 0; return NULL; }
	if (isLookingAtMe(p)) {
		if (++stareTicks >= 5) {      // a quarter of a second of eye contact
			stareTicks = 0;
			setCreepy(true);
			return p;
		}
	} else {
		stareTicks = 0;
	}
	return NULL;
}

void Enderman::checkHurtTarget(Entity* target, float d) {
	// far away: close the gap by teleporting now and then
	if (d > 16 && random.nextInt(10) == 0) teleportTowards(target);
	super::checkHurtTarget(target, d);
}

void Enderman::aiStep() {
	if (!level->isClientSide) {
		if (teleportCooldown > 0) teleportCooldown--;

		// water hurts: teleport away
		if (isInWater()) {
			hurt(NULL, 1);
			teleportRandom();
		}

		// daylight in the open: wander off now and then and calm down
		if (level->isDay() && random.nextInt(60) == 0) {
			const float br = getBrightness(1);
			if (br > 0.5f && level->canSeeSky(Mth::floor(x), Mth::floor(y), Mth::floor(z)) && random.nextFloat() * 30 < (br - 0.4f) * 2) {
				setAttackTarget(NULL);
				setCreepy(false);
				teleportRandom();
			}
		}

		// pick up / put down blocks
		const int carried = getCarriedTile();
		if (carried == 0) {
			if (random.nextInt(20) == 0) {
				const int tx = Mth::floor(x - 2 + random.nextFloat() * 4);
				const int ty = Mth::floor(y + random.nextFloat() * 3);
				const int tz = Mth::floor(z - 2 + random.nextFloat() * 4);
				const int t = level->getTile(tx, ty, tz);
				if (canCarry(t)) {
					setCarried(t, level->getData(tx, ty, tz));
					level->setTile(tx, ty, tz, 0);
				}
			}
		} else if (random.nextInt(2000) == 0) {
			const int tx = Mth::floor(x - 1 + random.nextFloat() * 2);
			const int ty = Mth::floor(y + random.nextFloat() * 2);
			const int tz = Mth::floor(z - 1 + random.nextFloat() * 2);
			const int here = level->getTile(tx, ty, tz), below = level->getTile(tx, ty - 1, tz);
			if (here == 0 && below > 0 && Tile::tiles[below] && Tile::tiles[below]->isCubeShaped()
				&& Tile::tiles[carried] && Tile::tiles[carried]->mayPlace(level, tx, ty, tz)) {
				level->setTileAndData(tx, ty, tz, carried, getCarriedData());
				setCarried(0, 0);
			}
		}

		if (getAttackTarget() == NULL && isCreepy()) setCreepy(false);
	}

	jumping = false;
	super::aiStep();
}

bool Enderman::hurt(Entity* source, int dmg) {
	// arrows never hit: it teleports out of the way
	if (source && source->getEntityTypeId() == EntityTypes::IdArrow) {
		for (int i = 0; i < 64 && !teleportRandom(); ++i) {}
		return false;
	}
	const bool hit = super::hurt(source, dmg);
	if (hit && !level->isClientSide && health > 0) {
		if (source && source->isPlayer()) setCreepy(true);
		if (random.nextInt(10) != 0) {
			for (int i = 0; i < 16 && !teleportRandom(); ++i) {}
		}
	}
	return hit;
}

bool Enderman::teleportRandom() {
	const float tx = x + (random.nextFloat() - 0.5f) * 64;
	const float ty = y + (float)(random.nextInt(64) - 32);
	const float tz = z + (random.nextFloat() - 0.5f) * 64;
	return teleport(tx, ty, tz);
}

bool Enderman::teleportTowards(Entity* e) {
	Vec3 d(x - e->x, bb.y0 + bbHeight * 0.5f - e->y, z - e->z);
	d = d.normalized();
	const float r = 16;
	return teleport(x + (random.nextFloat() - 0.5f) * 8 - d.x * r,
	                y + (float)(random.nextInt(16) - 8) - d.y * r,
	                z + (random.nextFloat() - 0.5f) * 8 - d.z * r);
}

bool Enderman::teleport(float tx, float ty, float tz) {
	if (teleportCooldown > 0) return false;
	const float ox = x, oy = y, oz = z;
	int bx = Mth::floor(tx), by = Mth::floor(ty), bz = Mth::floor(tz);
	if (by < 1) by = 1;
	if (by > Level::DEPTH - 3) by = Level::DEPTH - 3;
	if (!level->hasChunkAt(bx, by, bz)) return false;
	// drop to the ground
	bool ground = false;
	while (!ground && by > 1) {
		const int below = level->getTile(bx, by - 1, bz);
		if (below > 0 && level->getMaterial(bx, by - 1, bz)->blocksMotion()) ground = true;
		else by--;
	}
	if (!ground) return false;
	setPos(bx + 0.5f, (float)by + heightOffset, bz + 0.5f);
	if (!level->getCubes(this, bb).empty() || level->containsAnyLiquid(bb)) {
		setPos(ox, oy, oz);
		return false;
	}
	xd = yd = zd = 0;
	teleportCooldown = 4;
	// purple sparkles along the jump
	for (int i = 0; i < 64; ++i) {
		const float f = i / 63.0f;
		const float px = ox + (x - ox) * f + (random.nextFloat() - 0.5f) * bbWidth * 2;
		const float py = oy + (y - oy) * f + random.nextFloat() * bbHeight - heightOffset;
		const float pz = oz + (z - oz) * f + (random.nextFloat() - 0.5f) * bbWidth * 2;
		level->addParticle(PARTICLETYPE(reddust), px, py, pz, 0.55f, 0.15f, 0.85f);
	}
	return true;
}

void Enderman::addAdditonalSaveData(CompoundTag* tag) {
	super::addAdditonalSaveData(tag);
	tag->putShort("carried", (short)getCarriedTile());
	tag->putShort("carriedData", (short)getCarriedData());
}

void Enderman::readAdditionalSaveData(CompoundTag* tag) {
	super::readAdditionalSaveData(tag);
	setCarried(tag->getShort("carried"), tag->getShort("carriedData"));
}
