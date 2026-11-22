#ifndef NET_MINECRAFT_WORLD_ENTITY_MONSTER__Enderman_H__
#define NET_MINECRAFT_WORLD_ENTITY_MONSTER__Enderman_H__

#include "Monster.h"

/**
 * Enderman (Java 1.7 / PE 0.9.0 behaviour): neutral until a player looks at
 * its head or hurts it, teleports when hurt, in water and sometimes in
 * daylight, and picks up and puts down grass, dirt, sand, gravel, flowers,
 * mushrooms, cacti, clay, melons, mycelium and podzol.
 */
class Enderman: public Monster
{
	typedef Monster super;
	static const int DATA_CARRY_ID   = 16; // short: carried tile id
	static const int DATA_CARRY_DATA = 17; // byte: carried tile data
	static const int DATA_CREEPY     = 18; // byte: angry (mouth open)
public:
	Enderman(Level* level);

	void aiStep();
	bool hurt(Entity* source, int dmg);
	int getMaxHealth();
	int getEntityTypeId() const;

	void addAdditonalSaveData(CompoundTag* tag);
	void readAdditionalSaveData(CompoundTag* tag);

	int  getCarriedTile() const;
	int  getCarriedData() const;
	void setCarried(int tile, int data);
	bool isCreepy() const;
	void setCreepy(bool v);

	static bool canCarry(int tile);

protected:
	Entity* findAttackTarget();
	void checkHurtTarget(Entity* target, float d);
	const char* getAmbientSound() { return NULL; }
	std::string getHurtSound() { return ""; }
	std::string getDeathSound() { return ""; }
	int getDeathLoot() { return 0; }

private:
	bool isLookingAtMe(Mob* player);
	bool teleportRandom();
	bool teleportTowards(Entity* e);
	bool teleport(float x, float y, float z);

	int stareTicks;
	int teleportCooldown;
};

#endif /*NET_MINECRAFT_WORLD_ENTITY_MONSTER__Enderman_H__*/
