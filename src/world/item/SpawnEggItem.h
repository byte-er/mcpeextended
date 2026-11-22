#ifndef NET_MINECRAFT_WORLD_ITEM__SpawnEggItem_H__
#define NET_MINECRAFT_WORLD_ITEM__SpawnEggItem_H__

#include "Item.h"
#include "ItemInstance.h"
#include "../entity/MobFactory.h"
#include "../entity/player/Player.h"
#include "../level/Level.h"
#include "../Facing.h"
#include <string>

/**
 * Spawn eggs (PE 0.9.0 / Java id 383). The data value is the mob type;
 * icons are 0.9.0's pre-coloured eggs, row 12 of gui/items.png.
 */
class SpawnEggItem: public Item
{
	typedef Item super;
public:
	struct Egg { int mob; const char* key; };
	static const Egg* eggs(int& count) {
		static const Egg list[] = {
			{ MobTypes::Chicken, "chicken" }, { MobTypes::Cow, "cow" }, { MobTypes::Pig, "pig" },
			{ MobTypes::Sheep, "sheep" }, { MobTypes::Creeper, "creeper" }, { MobTypes::Skeleton, "skeleton" },
			{ MobTypes::Spider, "spider" }, { MobTypes::Zombie, "zombie" }, { MobTypes::PigZombie, "pigzombie" },
			{ MobTypes::Enderman, "enderman" },
		};
		count = (int)(sizeof(list) / sizeof(list[0]));
		return list;
	}
	static int indexOf(int mob) {
		int n; const Egg* e = eggs(n);
		for (int i = 0; i < n; ++i) if (e[i].mob == mob) return i;
		return -1;
	}

	SpawnEggItem(int id) : super(id) {
		setStackedByData(true);
		setMaxDamage(0);
	}

	int getIcon(int aux) {
		const int i = indexOf(aux);
		return i >= 0 ? i + 12 * ICON_COLUMNS : icon;
	}

	std::string getDescriptionId(const ItemInstance* inst) const {
		const int i = inst ? indexOf(inst->getAuxValue()) : -1;
		int n; const Egg* e = eggs(n);
		return super::getDescriptionId() + "." + (i >= 0 ? e[i].key : "unknown");
	}

	bool useOn(ItemInstance* inst, Player* player, Level* level, int x, int y, int z, int face, float cx, float cy, float cz) {
		if (level->isClientSide) return true;   // servers spawn it themselves
		x += Facing::STEP_X[face]; y += Facing::STEP_Y[face]; z += Facing::STEP_Z[face];
		Mob* mob = MobFactory::CreateMob(inst->getAuxValue(), level);
		if (!mob) return false;
		mob->moveTo(x + 0.5f, (float)y, z + 0.5f, level->random.nextFloat() * 360.0f, 0);
		level->addEntity(mob);
		if (!player->abilities.instabuild) inst->count--;
		return true;
	}
};

#endif /*NET_MINECRAFT_WORLD_ITEM__SpawnEggItem_H__*/
