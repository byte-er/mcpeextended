#include "PlayerNPC.h"
#include "NpcChat.h"
#include "../EntityTypes.h"
#include "../SharedFlags.h"
#include "../player/Player.h"
#include "../projectile/Arrow.h"
#include "../../level/Level.h"
#include "../../level/storage/LevelData.h"
#include "../../level/tile/Tile.h"
#include "../../item/Item.h"
#include "../../../nbt/CompoundTag.h"
#include "../../../util/Mth.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

using namespace NpcChat;

namespace {
int s_lastChatTime = -100000;   // level time of the last line from any NPC
const char* const ROLE_NAMES[PlayerNPC::ROLE_COUNT] = { "builder", "miner", "lumberjack", "farmer", "explorer", "griefer" };
}

// ---------------------------------------------------------------------------

PlayerNPC::PlayerNPC(Level* level)
:	super(level),
	number(0),
	role(ROLE_BUILDER),
	homeX(0), homeY(0), homeZ(0), hasHome(false), houseDone(false),
	planX(0), planY(0), planZ(0), planW(0), planD(0),
	farmX(0), farmY(0), farmZ(0), farmW(0), farmD(0),
	penX(0), penY(0), penZ(0), penW(0), penD(0),
	plotI(0), plotJ(0), hasPlot(false),
	job(JOB_NONE), jobTicks(0),
	axe(0), pick(0), hoe(0), sword(0),
	routeTx(0), routeTy(0), routeTz(0), routeAge(0), routeFails(0), stuckTicks(0),
	lastX(0), lastZ(0), digProgress(0), digNeeded(0), swingTimer(0),
	tableX(0), tableY(0), tableZ(0), hasTable(false),
	farmDone(false), penDone(false), waterPlaced(false), extrasBuilt(0),
	planPos(0), planSeed(0),
	mineX(0), mineY(0), mineZ(0), mineDir(0), mineDepth(0),
	fightTarget(0), helpingId(0), hasWater(false),
	chatCooldown(200), pendingTopic(-1), pendingDelay(0), lastTopicSaid(-1),
	keepLogs(0), seedTries(0), progTimer(0), progDist(1e9f), directTicks(0),
	standValid(false), standX(0), standY(0), standZ(0), standForX(0), standForY(0), standForZ(0),
	hasMine(false), mineFails(0), buildPasses(0), announcedBuild(false), exploreCount(0), exploreAngle(0),
	lastLoggedJob(-1), lastLogTime(0),
	caveMoves(0), caveTorchX(0), caveTorchY(-100), caveTorchZ(0), caveCooldown(0), returning(false),
	griefCooldown(20 * 60 * 3), lastGriefVictim(0), projectId(-1), storeCooldown(0), timeAlive(0),
	moveYaw(0), glance(0), glanceTarget(0), glanceTicks(0), glancing(false),
	hopHabit(0), hopTimer(0), idleFidget(0), swapTicks(0), sneakNow(false), sneakWas(false),
	greetCooldown(200), sprinting(false)
{
	entityRendererId = ER_NPC_RENDERER;
	textureName = "mob/char.png";
	setSize(0.6f, 1.8f);
	heightOffset = 1.62f;
	runSpeed = 0.75f;
	for (int i = 0; i < I_COUNT; ++i) inv[i] = 0;
	hopHabit = random.nextInt(3) == 0; // some players bunny hop everywhere
	entityData.define(DATA_NPC_NUMBER, (SynchedEntityData::TypeInt) 0); // LAN clients get the name from this
}

void PlayerNPC::setNumber(int n)
{
	number = n;
	entityData.set(DATA_NPC_NUMBER, (SynchedEntityData::TypeInt) n);
}

PlayerNPC::~PlayerNPC() {}

int PlayerNPC::getEntityTypeId() const { return MobTypes::PlayerNPC; }

std::string PlayerNPC::getNpcName() const
{
	char buf[32];
	sprintf(buf, "Player%d", number ? number : entityData.getInt(DATA_NPC_NUMBER));
	return buf;
}

float PlayerNPC::getWalkingSpeedModifier()
{
	float s = super::getWalkingSpeedModifier();
	if (sprinting) s *= 1.3f;
	if (isSneaking()) s *= 0.3f;
	return s;
}

PlayerNPC* PlayerNPC::spawn(Level* level, float x, float y, float z)
{
	PlayerNPC* npc = new PlayerNPC(level);
	npc->setNumber(++level->getLevelData()->npcCounter);
	// everybody plays a little differently
	static const int weights[ROLE_COUNT] = { 26, 20, 15, 14, 15, 10 };
	int r = level->random.nextInt(100);
	npc->role = ROLE_BUILDER;
	for (int i = 0; i < ROLE_COUNT; ++i) { if (r < weights[i]) { npc->role = i; break; } r -= weights[i]; }
	npc->moveTo(x, y + npc->heightOffset, z, level->random.nextFloat() * 360, 0);
	level->addEntity(npc);
	level->chatMessage("[yellow]" + npc->getNpcName() + " joined the game[/yellow]");
	// the others notice, the new one says hi a bit later
	NpcTown* town = NpcTown::get(level);
	const int pop = town->population();
	std::vector<PlayerNPC*> o = npc->others();
	if (!o.empty() && pop < 12 && level->random.nextInt(2) == 0) o[level->random.nextInt((int)o.size())]->reply(T_HELLO, npc);
	if (level->random.nextInt(pop < 12 ? 3 : 8) == 0 || pop < 3) {
		npc->pendingLine = SAY_LINES[T_HELLO][level->random.nextInt(6)]; npc->pendingTopic = -1; npc->pendingDelay = 60 + level->random.nextInt(140);
	}
	return npc;
}

std::vector<PlayerNPC*> PlayerNPC::others()
{
	const std::vector<PlayerNPC*>& all = NpcTown::get(level)->npcs();
	std::vector<PlayerNPC*> out;
	out.reserve(all.size());
	for (unsigned int i = 0; i < all.size(); ++i) if (all[i] != this && !all[i]->removed && all[i]->isAlive()) out.push_back(all[i]);
	return out;
}

PlayerNPC* PlayerNPC::nearestOther(float maxDist)
{
	const std::vector<PlayerNPC*>& all = NpcTown::get(level)->npcs();
	PlayerNPC* best = NULL;
	float bd = maxDist * maxDist;
	for (unsigned int i = 0; i < all.size(); ++i) {
		PlayerNPC* n = all[i];
		if (n == this || n->removed || !n->isAlive()) continue;
		const float d = n->distanceToSqr(this);
		if (d < bd) { bd = d; best = n; }
	}
	return best;
}

// ---------------------------------------------------------------------------
// damage: the real player can't hurt them; everything else can (other NPCs too)
// ---------------------------------------------------------------------------
bool PlayerNPC::hurt(Entity* source, int dmg)
{
	// the real player can't hurt them (hands, weapons or arrows)
	if (source && source->isPlayer()) return false;
	if (source && source->getEntityTypeId() == EntityTypes::IdArrow) {
		Entity* owner = level->getEntity(((Arrow*)source)->ownerId);
		if (!owner || owner->isPlayer()) return false;
	}
	const bool hit = super::hurt(source, dmg);
	if (hit && source && source->isMob() && source->isAlive()) {
		const bool byNpc = source->getEntityTypeId() == MobTypes::PlayerNPC;
		if (byNpc) {
			PlayerNPC* other = (PlayerNPC*)source;
			for (unsigned int i = 0; i < grudges.size(); ++i) if (grudges[i].number == other->number) { grudges[i].amount += 2; other = NULL; break; }
			if (other) { Grudge g = { other->number, 2 }; grudges.push_back(g); }
		}
		// low on health and it's another player: get away (griefers know when to run)
		if (byNpc && health <= 8 && (role == ROLE_GRIEFER || sword == 0)) {
			if (job != JOB_FLEE) { steps.clear(); planFlee(source); }
			if (random.nextInt(2) == 0) say(role == ROLE_GRIEFER ? SAY_LINES[T_SORRY][random.nextInt(6)] : SAY_LINES[T_HURT][random.nextInt(6)], role == ROLE_GRIEFER ? T_SORRY : T_HURT);
			return hit;
		}
		fightTarget = source->entityId;
		if (job != JOB_FIGHT && job != JOB_FLEE) { steps.clear(); job = JOB_FIGHT; jobTicks = 0; }
		if (random.nextInt(3) == 0) say(SAY_LINES[T_HURT][random.nextInt(6)], T_HURT);
	}
	return hit;
}

void PlayerNPC::die(Entity* source)
{
	if (!level->isClientSide && health <= 0) {
		std::string cause = " died";
		if (source && source->getEntityTypeId() == MobTypes::PlayerNPC) {
			cause = " was slain by " + ((PlayerNPC*)source)->getNpcName();
		} else if (source && source->isMob()) {
			const int t = source->getEntityTypeId();
			const char* n = t == MobTypes::Zombie ? "Zombie" : t == MobTypes::Skeleton ? "Skeleton" : t == MobTypes::Spider ? "Spider"
				: t == MobTypes::Creeper ? "Creeper" : t == MobTypes::Enderman ? "Enderman" : t == MobTypes::PigZombie ? "Zombie Pigman" : "a mob";
			cause = std::string(" was slain by ") + n;
		} else if (isInLava()) cause = " tried to swim in lava";
		else if (isInWater() && airSupply <= 0) cause = " drowned";
		else if (fallDistance > 3) cause = " fell from a high place";
		level->chatMessage(getNpcName() + cause);
		if (getenv("MCPE_NPCLOG")) { printf("[npc] %s%s at %.1f,%.1f,%.1f job %d\n", getNpcName().c_str(), cause.c_str(), x, y - heightOffset, z, job); fflush(stdout); }
		std::vector<PlayerNPC*> o = others();
		if (!o.empty()) o[random.nextInt((int)o.size())]->reply(T_DEATH, this);
		// drops what it carried, like a player
		if (inv[I_LOG] > 0) spawnAtLocation(new ItemInstance(Tile::treeTrunk, Mth::Min(64, inv[I_LOG])), 0.5f);
		if (inv[I_PLANK] > 0) spawnAtLocation(new ItemInstance(Tile::wood, Mth::Min(64, inv[I_PLANK])), 0.5f);
		if (inv[I_COBBLE] > 0) spawnAtLocation(new ItemInstance(Tile::stoneBrick, Mth::Min(64, inv[I_COBBLE])), 0.5f);
		if (inv[I_WHEAT] > 0) spawnAtLocation(new ItemInstance(Item::wheat, Mth::Min(64, inv[I_WHEAT])), 0.5f);
		if (inv[I_COAL] > 0) spawnAtLocation(new ItemInstance(Item::coal, Mth::Min(64, inv[I_COAL])), 0.5f);
		if (inv[I_INGOT] > 0) spawnAtLocation(new ItemInstance(Item::ironIngot, Mth::Min(64, inv[I_INGOT])), 0.5f);
		if (inv[I_DIAMOND] > 0) spawnAtLocation(new ItemInstance(Item::emerald, Mth::Min(64, inv[I_DIAMOND])), 0.5f);
		if (inv[I_TORCH] > 0) spawnAtLocation(new ItemInstance(Tile::torch, Mth::Min(64, inv[I_TORCH])), 0.5f);
		if (inv[I_TABLE] > 0) spawnAtLocation(new ItemInstance(Tile::workBench, 1), 0.5f);
		if (pick > 0) spawnAtLocation(new ItemInstance(pick == 4 ? Item::pickAxe_emerald : pick == 3 ? Item::pickAxe_iron : pick == 2 ? Item::pickAxe_stone : Item::pickAxe_wood), 0.5f);
		if (sword > 0) spawnAtLocation(new ItemInstance(sword == 3 ? Item::sword_iron : sword == 2 ? Item::sword_stone : Item::sword_wood), 0.5f);
	}
	super::die(source);
}

int PlayerNPC::grudgeAgainst(int n) const
{
	for (unsigned int i = 0; i < grudges.size(); ++i) if (grudges[i].number == n) return grudges[i].amount;
	return 0;
}

// someone griefed: the victim and whoever was around remember it
void PlayerNPC::heardGrief(PlayerNPC* griefer, int victim, int gx, int gy, int gz, bool saw)
{
	if (griefer == this) return;
	const int add = (victim == number) ? 3 : saw ? 2 : 1;
	bool found = false;
	for (unsigned int i = 0; i < grudges.size(); ++i) if (grudges[i].number == griefer->number) { grudges[i].amount += add; found = true; }
	if (!found) { Grudge g = { griefer->number, add }; grudges.push_back(g); }
	NpcTown* town = NpcTown::get(level);
	if (victim == number) {
		// my house: say something and fix it
		if (town->mayChat(2) && random.nextInt(3) != 0) {
			if (saw && random.nextInt(2) == 0) say(griefer->getNpcName() + " " + SAY_LINES[T_ACCUSE][random.nextInt(6)], T_ACCUSE);
			else say(SAY_LINES[T_GRIEFED][random.nextInt(6)], T_GRIEFED);
		}
		if (houseDone && planW > 0) { houseDone = false; makeHouseDesign(planSeed); planPos = 0; announcedBuild = true; }
	} else if (saw && role != ROLE_GRIEFER && random.nextInt(4) == 0 && town->mayChat(1)) {
		say(SAY_LINES[T_ACCUSE][random.nextInt(6)] + (random.nextInt(2) ? " " + griefer->getNpcName() : ""), T_ACCUSE);
	}
	// enough is enough: go after them (if armed and close)
	const int g = grudgeAgainst(griefer->number);
	if (role != ROLE_GRIEFER && g >= 4 && sword > 0 && griefer->distanceToSqr(this) < 24 * 24 && job != JOB_FIGHT) {
		steps.clear();
		fightTarget = griefer->entityId;
		job = JOB_NONE;
	}
}

// ---------------------------------------------------------------------------
// main loop
// ---------------------------------------------------------------------------
void PlayerNPC::aiStep()
{
	super::aiStep();
	updateAttackAnim(); // the arm swing shows like a player's
	if (swingTimer > 0) swingTimer--;
}

void PlayerNPC::updateAi()
{
	noActionTime = 0;
	if (glancing) { yRot = moveYaw; glancing = false; } // walking code works on the real heading
	xxa = 0;
	yya = 0;
	jumping = false;
	sprinting = false;
	sneakNow = false;
	if (level->isClientSide) return;

	NpcTown* town = NpcTown::get(level);
	town->tick();
	chatTick();
	++jobTicks;
	++timeAlive;
	if (griefCooldown > 0) griefCooldown--;
	if (storeCooldown > 0) storeCooldown--;
	if (caveCooldown > 0) caveCooldown--;
	if (greetCooldown > 0) greetCooldown--;
	// grudges fade (slowly)
	if ((timeAlive % (20 * 120)) == 0)
		for (int i = (int)grudges.size() - 1; i >= 0; --i) if (--grudges[i].amount <= 0) grudges.erase(grudges.begin() + i);

	// players heal while fed; eats some bread when hurt
	if (health < getMaxHealth() && health > 0 && (tickCount % 80) == 0) {
		heal(1);
		if (health < 12 && inv[I_BREAD] > 0 && random.nextInt(4) == 0) { inv[I_BREAD]--; heal(5); level->playSound(this, "random.eat", 0.5f, 1.0f); }
	}

	// sand or gravel fell on me: dig out, like anyone would
	if (isInWall()) {
		const int fx = Mth::floor(x), fz = Mth::floor(z), ey = Mth::floor(y), fy = Mth::floor(y - heightOffset);
		const int ty = level->getMaterial(fx, ey, fz)->blocksMotion() ? ey : fy;
		if (steps.empty() || steps.front().kind != Step::BREAK || steps.front().x != fx || steps.front().y != ty || steps.front().z != fz)
			pushFront(Step::BREAK, fx, ty, fz, -1);
	}

	// lost or stuck in a job for too long: rethink
	if (jobTicks > 20 * 180 && job != JOB_NIGHT) { steps.clear(); job = JOB_NONE; }
	if (steps.empty()) think();
	static const bool log = getenv("MCPE_NPCLOG") != NULL; // development
	if (log && !steps.empty() && (tickCount % 400) == 0) {
		const Step& st = steps.front();
		printf("[npc] %s   step kind=%d at %d,%d,%d tile=%d (%d left) pos %.1f,%.1f,%.1f fails=%d direct=%d\n", getNpcName().c_str(), st.kind, st.x, st.y, st.z, st.tile,
			(int)steps.size(), x, y - heightOffset, z, routeFails, directTicks);
		fflush(stdout);
	}
	if (!steps.empty()) runStep();
	else held = ItemInstance();

	// swim, and never stand in place underwater
	if (isInWater() || isInLava()) jumping = true;

	microTick();
	applyGlance();
	setSneak(sneakNow);
}

// ---------------------------------------------------------------------------
// the small things real players do: look around while walking, sprint and hop,
// fidget while waiting, crouch at each other, scroll through the hotbar
// ---------------------------------------------------------------------------
void PlayerNPC::microTick()
{
	const bool moving = yya > 0.01f || xxa > 0.01f || xxa < -0.01f;
	// sprint-jumping when going somewhere far
	if (sprinting && onGround && hopHabit && moving && !horizontalCollision) {
		if (--hopTimer <= 0) { jumping = true; hopTimer = 6 + random.nextInt(14); }
	}
	// glance around while walking (the body keeps going the same way)
	if (moving) {
		if (glanceTicks > 0) { if (--glanceTicks == 0) glanceTarget = 0; }
		else if (random.nextInt(70) == 0) {
			PlayerNPC* o = nearestOther(10);
			if (o && random.nextInt(2) == 0) {
				float a = (float)(Mth::atan2(o->z - z, o->x - x) * 180 / Mth::PI) - 90 - yRot;
				while (a < -180) a += 360;
				while (a >= 180) a -= 360;
				glanceTarget = Mth::clamp(a, -75.0f, 75.0f);
			} else glanceTarget = (random.nextInt(2) ? 1 : -1) * (20.0f + random.nextFloat() * 45.0f);
			glanceTicks = 10 + random.nextInt(25);
			if (random.nextInt(3) == 0) xRot = random.nextFloat() * 40 - 15;
		}
	} else glanceTarget = 0;
	glance += (glanceTarget - glance) * 0.25f;

	// greet someone passing by: look at them and crouch a couple of times
	if (idleFidget > 0) {
		idleFidget--;
		sneakNow = ((idleFidget / 3) & 1) != 0;
	} else if (greetCooldown == 0 && !moving && random.nextInt(40) == 0) {
		PlayerNPC* o = nearestOther(4);
		greetCooldown = 20 * 20 + random.nextInt(20 * 40);
		if (o && random.nextInt(3) == 0) {
			lookAt(o, 30, 30);
			idleFidget = 18;
		}
	}

	// now and then scroll through the hotbar while walking
	if (swapTicks > 0) {
		if (--swapTicks == 0) held = swapBack;
		else if ((swapTicks % 4) == 0) {
			static Item* const* const tools[] = { &Item::pickAxe_stone, &Item::hatchet_stone, &Item::sword_stone, &Item::shovel_stone };
			const int k = random.nextInt(4);
			if (k == 3) held = ItemInstance(Tile::stoneBrick);
			else held = ItemInstance(*tools[k]);
		}
	} else if (moving && random.nextInt(1500) == 0) {
		swapBack = held;
		swapTicks = 4 * (2 + random.nextInt(3)) + 1;
	}
}

void PlayerNPC::applyGlance()
{
	if (glance > -1 && glance < 1) return;
	if (yya < 0.01f && xxa > -0.01f && xxa < 0.01f) return;
	// face a bit to the side but keep walking where we were going
	moveYaw = yRot;
	yRot = moveYaw + glance;
	const float g = glance * Mth::PI / 180;
	const float fwd = yya, side = xxa;
	yya = fwd * Mth::cos(g) - side * Mth::sin(g);
	xxa = fwd * Mth::sin(g) + side * Mth::cos(g);
	glancing = true;
}

void PlayerNPC::setSneak(bool on)
{
	if (on == sneakWas) return;
	sneakWas = on;
	setSharedFlag(SharedFlagsInformation::FLAG_SNEAKING, on);
}

// ---------------------------------------------------------------------------
// chat
// ---------------------------------------------------------------------------
void PlayerNPC::say(const std::string& text, int topic)
{
	if (text.empty()) return;
	NpcTown* town = NpcTown::get(level);
	const int imp = topic >= 0 ? importance(topic) : 1;
	if (!town->mayChat(imp)) return; // lots of people online: not everything gets said
	const int now = (int)level->getTime();
	if (now - s_lastChatTime < 30 && topic >= 0 && imp < 2) {
		// someone just talked: say it a little later
		pendingLine = text; pendingTopic = topic; pendingDelay = 40 + random.nextInt(60);
		return;
	}
	s_lastChatTime = now;
	level->chatMessage("<" + getNpcName() + "> " + text);
	chatCooldown = 20 * 30 + random.nextInt(20 * 60);
	if (topic >= 0) {
		lastTopicSaid = topic;
		// maybe someone answers
		std::vector<PlayerNPC*> o = others();
		if (!o.empty() && random.nextInt(town->population() > 12 ? 4 : 3) != 0) o[random.nextInt((int)o.size())]->reply(topic, this);
	}
}

void PlayerNPC::reply(int topic, PlayerNPC* to)
{
	if (topic < 0 || topic >= T_COUNT) return;
	std::string line = REPLY_LINES[topic][random.nextInt(4)];
	if (line.empty()) return;
	if (topic == T_HELLO && to && random.nextInt(2) == 0) line += " " + to->getNpcName();
	if (topic == T_DEATH && to) line = (random.nextInt(2) ? "rip " : "noo ") + to->getNpcName();
	if (topic == T_GRIEF && to && grudgeAgainst(to->number) > 0) line = to->getNpcName() + " stop";
	if (!pendingLine.empty()) return;
	pendingLine = line;
	pendingTopic = -1;
	pendingDelay = 30 + random.nextInt(80);
}

void PlayerNPC::chatTick()
{
	if (chatCooldown > 0) chatCooldown--;
	if (!pendingLine.empty() && --pendingDelay <= 0) {
		const std::string line = pendingLine;
		const int topic = pendingTopic;
		pendingLine.clear();
		s_lastChatTime = -100000; // let it through
		say(line, topic);
	}
	// now and then, just talk
	if (chatCooldown == 0 && random.nextInt(20 * 90) == 0) {
		if (role == ROLE_GRIEFER && random.nextInt(3) == 0) say(SAY_LINES[T_GRIEF][random.nextInt(6)], T_BANTER);
		else say(SAY_LINES[T_BANTER][random.nextInt(6)], T_BANTER);
	}
	// night: go home (miners and explorers would rather go caving)
	if (!level->isDay() && job != JOB_NIGHT && job != JOB_CAVE && job != JOB_FIGHT && houseDone && random.nextInt(20 * 20) == 0) {
		if (!((role == ROLE_MINER || role == ROLE_EXPLORER) && random.nextInt(2) == 0)) { steps.clear(); job = JOB_NONE; }
	}
}

// ---------------------------------------------------------------------------
// infinite worlds: the window moved
// ---------------------------------------------------------------------------
void PlayerNPC::windowShifted(int dx, int dz)
{
	homeX += dx; homeZ += dz;
	tableX += dx; tableZ += dz;
	planX += dx; planZ += dz;
	farmX += dx; farmZ += dz;
	penX += dx; penZ += dz;
	mineX += dx; mineZ += dz;
	caveTorchX += dx; caveTorchZ += dz;
	for (unsigned int i = 0; i < plan.size(); ++i) { plan[i].x += dx; plan[i].z += dz; }
	for (unsigned int i = 0; i < steps.size(); ++i) { steps[i].x += dx; steps[i].z += dz; }
	for (unsigned int i = 0; i < caveTrail.size(); ++i) { caveTrail[i].x += dx; caveTrail[i].z += dz; }
	for (unsigned int i = 0; i < badSpots.size(); ++i) { badSpots[i].x += dx; badSpots[i].z += dz; }
	route.destroy();
	// town projects are in world coordinates: their plans need making again
	NpcTown* town = NpcTown::get(level);
	for (unsigned int i = 0; i < town->projects.size(); ++i) { town->projects[i].plan.clear(); town->projects[i].built = false; }
	steps.clear();
}

// ---------------------------------------------------------------------------
// saving: places are stored relative to the NPC (infinite worlds move)
// ---------------------------------------------------------------------------
void PlayerNPC::addAdditonalSaveData(CompoundTag* tag)
{
	super::addAdditonalSaveData(tag);
	const int bx = Mth::floor(x), by = Mth::floor(y), bz = Mth::floor(z);
	tag->putInt("Number", number);
	tag->putInt("Role", role);
	for (int i = 0; i < I_COUNT; ++i) { char k[16]; sprintf(k, "Inv%d", i); tag->putInt(k, inv[i]); }
	tag->putInt("Axe", axe); tag->putInt("Pick", pick); tag->putInt("Hoe", hoe); tag->putInt("Sword", sword);
	tag->putByte("HasHome", hasHome); tag->putInt("HomeX", homeX - bx); tag->putInt("HomeY", homeY - by); tag->putInt("HomeZ", homeZ - bz);
	tag->putByte("HasTable", hasTable); tag->putInt("TableX", tableX - bx); tag->putInt("TableY", tableY - by); tag->putInt("TableZ", tableZ - bz);
	tag->putByte("HouseDone", houseDone); tag->putByte("FarmDone", farmDone); tag->putByte("PenDone", penDone); tag->putByte("Water", waterPlaced);
	tag->putInt("Extras", extrasBuilt);
	const bool building = !plan.empty() && job == JOB_BUILD;
	tag->putByte("Building", building);
	tag->putInt("PlanSeed", planSeed); tag->putInt("PlanPos", (int)planPos);
	tag->putInt("PlanX", planX - bx); tag->putInt("PlanY", planY - by); tag->putInt("PlanZ", planZ - bz); tag->putInt("PlanW", planW); tag->putInt("PlanD", planD);
	tag->putInt("FarmX", farmX - bx); tag->putInt("FarmY", farmY - by); tag->putInt("FarmZ", farmZ - bz); tag->putInt("FarmW", farmW); tag->putInt("FarmD", farmD);
	tag->putInt("PenX", penX - bx); tag->putInt("PenY", penY - by); tag->putInt("PenZ", penZ - bz); tag->putInt("PenW", penW); tag->putInt("PenD", penD);
	tag->putInt("MineX", mineX - bx); tag->putInt("MineY", mineY - by); tag->putInt("MineZ", mineZ - bz); tag->putInt("MineDir", mineDir); tag->putByte("HasMine", hasMine); tag->putInt("MineDepth", mineDepth);
	tag->putByte("HasPlot", hasPlot); tag->putInt("PlotI", plotI); tag->putInt("PlotJ", plotJ);
	std::string g;
	for (unsigned int i = 0; i < grudges.size(); ++i) { char b[32]; sprintf(b, "%s%d:%d", i ? "," : "", grudges[i].number, grudges[i].amount); g += b; }
	tag->putString("Grudges", g);
	NpcTown* town = NpcTown::get(level);
	if (town->active) {
		CompoundTag* t = new CompoundTag();
		town->save(t);
		tag->putCompound("Town", t);
	}
}

void PlayerNPC::readAdditionalSaveData(CompoundTag* tag)
{
	super::readAdditionalSaveData(tag);
	const int bx = Mth::floor(x), by = Mth::floor(y), bz = Mth::floor(z);
	setNumber(tag->getInt("Number"));
	role = tag->contains("Role") ? Mth::clamp(tag->getInt("Role"), 0, ROLE_COUNT - 1) : ROLE_BUILDER;
	for (int i = 0; i < I_COUNT; ++i) { char k[16]; sprintf(k, "Inv%d", i); inv[i] = tag->contains(k) ? tag->getInt(k) : 0; }
	axe = tag->getInt("Axe"); pick = tag->getInt("Pick"); hoe = tag->getInt("Hoe"); sword = tag->getInt("Sword");
	hasHome = tag->getByte("HasHome") != 0; homeX = tag->getInt("HomeX") + bx; homeY = tag->getInt("HomeY") + by; homeZ = tag->getInt("HomeZ") + bz;
	hasTable = tag->getByte("HasTable") != 0; tableX = tag->getInt("TableX") + bx; tableY = tag->getInt("TableY") + by; tableZ = tag->getInt("TableZ") + bz;
	houseDone = tag->getByte("HouseDone") != 0; farmDone = tag->getByte("FarmDone") != 0; penDone = tag->getByte("PenDone") != 0; waterPlaced = tag->getByte("Water") != 0;
	extrasBuilt = tag->getInt("Extras");
	planSeed = tag->getInt("PlanSeed");
	planX = tag->getInt("PlanX") + bx; planY = tag->getInt("PlanY") + by; planZ = tag->getInt("PlanZ") + bz; planW = tag->getInt("PlanW"); planD = tag->getInt("PlanD");
	farmX = tag->getInt("FarmX") + bx; farmY = tag->getInt("FarmY") + by; farmZ = tag->getInt("FarmZ") + bz; farmW = tag->getInt("FarmW"); farmD = tag->getInt("FarmD");
	penX = tag->getInt("PenX") + bx; penY = tag->getInt("PenY") + by; penZ = tag->getInt("PenZ") + bz; penW = tag->getInt("PenW"); penD = tag->getInt("PenD");
	mineX = tag->getInt("MineX") + bx; mineY = tag->getInt("MineY") + by; mineZ = tag->getInt("MineZ") + bz; mineDir = tag->getInt("MineDir"); hasMine = tag->getByte("HasMine") != 0; mineFails = 0; mineDepth = tag->getInt("MineDepth");
	hasPlot = tag->getByte("HasPlot") != 0; plotI = tag->getInt("PlotI"); plotJ = tag->getInt("PlotJ");
	grudges.clear();
	const std::string g = tag->getString("Grudges");
	for (size_t p = 0; p < g.size();) {
		int n = 0, a = 0;
		if (sscanf(g.c_str() + p, "%d:%d", &n, &a) == 2 && n > 0 && a > 0) { Grudge gr = { n, a }; grudges.push_back(gr); }
		const size_t c = g.find(',', p);
		if (c == std::string::npos) break;
		p = c + 1;
	}
	if (tag->contains("Town")) {
		NpcTown* town = NpcTown::get(level);
		if (!town->loaded) town->load(tag->getCompound("Town"));
	}
	// older saves: the table it had standing around is the house's now
	if (!houseDone && planW > 0) {
		// the same house as before, picking up where it was
		makeHouseDesign(planSeed);
		planPos = 0; // finished blocks are skipped automatically
	}
}
