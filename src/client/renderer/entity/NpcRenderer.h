#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY__NpcRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY__NpcRenderer_H__

#include "HumanoidMobRenderer.h"
#include "../../model/HumanoidModel.h"
#include "../../../world/entity/npc/PlayerNPC.h"

/** Player NPCs: Steve with the held item and a "PlayerN" name tag, like a remote player. */
class NpcRenderer : public HumanoidMobRenderer
{
	typedef HumanoidMobRenderer super;
public:
	NpcRenderer() : super(new HumanoidModel(), 0.5f) {}
protected:
	void renderName(Mob* mob, float x, float y, float z) {
		renderNameTag(mob, ((PlayerNPC*)mob)->getNpcName(), x, y, z, 32);
	}
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_ENTITY__NpcRenderer_H__*/
