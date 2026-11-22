#ifndef NET_MINECRAFT_CLIENT_MODEL__EndermanModel_H__
#define NET_MINECRAFT_CLIENT_MODEL__EndermanModel_H__

#include "HumanoidModel.h"
#include "geom/ModelPart.h"
#include "../../util/Mth.h"

// Java's ModelEnderman: long thin limbs, raised body, a jaw that drops when angry
class EndermanModel: public HumanoidModel
{
	typedef HumanoidModel super;
public:
	bool carrying;
	bool creepy;
	ModelPart hat; // the black head with the mouth

	EndermanModel()
	:	super(0, -14),
		carrying(false),
		creepy(false),
		hat(0, 16)
	{
		const float f = -14;
		hat.setModel(this);
		hat.addBox(-4, -8, -4, 8, 8, 8, -0.5f);
		hat.setPos(0, f, 0);

		body = ModelPart(32, 16);
		body.setModel(this);
		body.addBox(-4, 0, -2, 8, 12, 4, 0);
		body.setPos(0, f, 0);

		arm0 = ModelPart(56, 0);
		arm0.setModel(this);
		arm0.addBox(-1, -2, -1, 2, 30, 2, 0);
		arm0.setPos(-3, 2 + f, 0);

		arm1 = ModelPart(56, 0);
		arm1.setModel(this);
		arm1.mirror = true;
		arm1.addBox(-1, -2, -1, 2, 30, 2, 0);
		arm1.setPos(5, 2 + f, 0);

		leg0 = ModelPart(56, 0);
		leg0.setModel(this);
		leg0.addBox(-1, 0, -1, 2, 30, 2, 0);
		leg0.setPos(-2, 12 + f, 0);

		leg1 = ModelPart(56, 0);
		leg1.setModel(this);
		leg1.mirror = true;
		leg1.addBox(-1, 0, -1, 2, 30, 2, 0);
		leg1.setPos(2, 12 + f, 0);
	}

	void setupAnim(float time, float r, float bob, float yRot, float xRot, float scale) {
		super::setupAnim(time, r, bob, yRot, xRot, scale);
		const float f = -14;
		body.xRot = 0;
		body.y = f;
		body.z = 0;
		arm0.xRot *= 0.5f; arm1.xRot *= 0.5f;
		leg0.xRot *= 0.5f; leg1.xRot *= 0.5f;
		const float lim = 0.4f;
		arm0.xRot = Mth::clamp(arm0.xRot, -lim, lim); arm1.xRot = Mth::clamp(arm1.xRot, -lim, lim);
		leg0.xRot = Mth::clamp(leg0.xRot, -lim, lim); leg1.xRot = Mth::clamp(leg1.xRot, -lim, lim);
		if (carrying) {
			arm0.xRot = -0.5f; arm1.xRot = -0.5f;
			arm0.zRot = 0.05f; arm1.zRot = -0.05f;
		}
		arm0.z = arm1.z = 0;
		leg0.z = leg1.z = 0;
		leg0.y = leg1.y = 9 + f;
		head.z = 0;
		head.y = f + 1;
		hat.x = head.x; hat.y = head.y; hat.z = head.z;
		hat.xRot = head.xRot; hat.yRot = head.yRot; hat.zRot = head.zRot;
		if (creepy) head.y -= 5; // the top of the head lifts: mouth open
	}

	void render(Entity* e, float time, float r, float bob, float yRot, float xRot, float scale) {
		setupAnim(time, r, bob, yRot, xRot, scale);
		head.render(scale);
		hat.render(scale);
		body.render(scale);
		arm0.render(scale);
		arm1.render(scale);
		leg0.render(scale);
		leg1.render(scale);
	}

	void onGraphicsReset() {
		super::onGraphicsReset();
		hat.onGraphicsReset();
	}
};

#endif /*NET_MINECRAFT_CLIENT_MODEL__EndermanModel_H__*/
