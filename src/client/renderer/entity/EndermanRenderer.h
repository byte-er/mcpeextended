#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY__EndermanRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY__EndermanRenderer_H__

#include "MobRenderer.h"
#include "../TileRenderer.h"
#include "../gles.h"
#include "../../model/EndermanModel.h"
#include "../../../world/entity/monster/Enderman.h"
#include "../../../world/level/tile/Tile.h"

class EndermanRenderer: public MobRenderer
{
	typedef MobRenderer super;
public:
	EndermanRenderer()
	:	super(endermanModel = new EndermanModel(), 0.5f),
		eyes(new EndermanModel()),
		tiles(new TileRenderer())
	{
		setArmor(eyes);
	}
	~EndermanRenderer() {
		delete eyes;
		delete tiles;
	}

	void render(Entity* e, float x, float y, float z, float rot, float a) {
		Enderman* m = (Enderman*)e;
		endermanModel->carrying = eyes->carrying = m->getCarriedTile() > 0;
		endermanModel->creepy = eyes->creepy = m->isCreepy();
		super::render(e, x, y, z, rot, a);
	}

protected:
	// glowing purple eyes, drawn additively and full bright over the body
	int prepareArmor(Mob* mob, int layer, float a) {
		if (layer != 0) return -1;
		bindTexture("mob/enderman_eyes.png");
		glEnable2(GL_BLEND);
		glDisable2(GL_ALPHA_TEST);
		glBlendFunc2(GL_ONE, GL_ONE);
		glColor4f2(1, 1, 1, 1);
		return 1;
	}

	void additionalRendering(Mob* mob, float a) {
		glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		Enderman* m = (Enderman*)mob;
		const int t = m->getCarriedTile();
		if (t <= 0 || t >= 256 || !Tile::tiles[t]) return;
		glPushMatrix2();
		glTranslatef2(0.0f, 0.6875f, -0.75f);
		glRotatef2(20.0f, 1, 0, 0);
		glRotatef2(45.0f, 0, 1, 0);
		const float s = 0.5f;
		glScalef2(-s, -s, s);
		bindTexture("terrain.png");
		const float br = mob->getBrightness(a);
		glColor4f2(br, br, br, 1);
		tiles->renderGuiTile(Tile::tiles[t], m->getCarriedData());
		glColor4f2(1, 1, 1, 1);
		glPopMatrix2();
	}

private:
	EndermanModel* endermanModel;
	EndermanModel* eyes;
	TileRenderer* tiles;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_ENTITY__EndermanRenderer_H__*/
