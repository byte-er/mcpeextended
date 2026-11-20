#include "EaglerClient.h"
#include "EaglerPacket.h"

#include "../../client/Minecraft.h"
#include "../../client/player/LocalPlayer.h"
#include "../../client/player/RemotePlayer.h"
#include "../../client/multiplayer/MultiPlayerLevel.h"
#include "../../client/renderer/LevelRenderer.h"
#include "../../client/gamemode/GameMode.h"
#include "../../client/gui/Gui.h"
#include "../../client/User.h"
#include "../../world/level/Level.h"
#include "../../world/level/LevelSettings.h"
#include "../../world/level/WorldBorder.h"
#include "../../world/level/chunk/LevelChunk.h"
#include "../../world/level/storage/LevelStorageSource.h"
#include "../../world/level/dimension/Dimension.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/item/Item.h"
#include "../../world/item/ItemInstance.h"
#include "../../world/entity/Entity.h"
#include "../../world/entity/Mob.h"
#include "../../world/entity/MobFactory.h"
#include "../../world/entity/item/ItemEntity.h"
#include "../../world/entity/player/Player.h"
#include "../../world/entity/player/Inventory.h"
#include "../../SharedConstants.h"
#include "../../AppPlatform.h"
#include "../../platform/log.h"
#include "../../util/Mth.h"
#include "../../client/skin/PlayerSkin.h"
#include "../../world/level/BiomeTint.h"
#include "../../client/gui/screens/JavaInventoryScreen.h"

#include <cmath>
#include <cstdlib>
#include <cctype>

// Window: 32x32 chunks (CHUNK_CACHE_WIDTH) = 512 blocks. Re-centre when the
// player is this far from the centre.
static const int WINDOW_CHUNKS = 32;
static const int WINDOW_HALF = WINDOW_CHUNKS * 16 / 2;   // 256
static const int RECENTRE_DIST = 128;
static const int HEIGHT = 128;

EaglerClient* EaglerClient::s_active = NULL;

// ---------------------------------------------------------------------------
// 1.8 block ids -> blocks this engine has
// ---------------------------------------------------------------------------
static unsigned char s_idMap[256];
static bool s_idMapReady = false;

static void initIdMap()
{
	if (s_idMapReady) return;
	s_idMapReady = true;
	for (int i = 0; i < 256; ++i) {
		if (Tile::tiles[i]) s_idMap[i] = (unsigned char)i;
		else s_idMap[i] = (unsigned char)(i < 198 ? 1 : 0); // unknown: solid stand-in
	}
	struct { int from, to; } m[] = {
		// ids that mean something else in Pocket Edition, or that it lacks
		{ 95, 20 },  { 160, 102 },          // stained glass (PE 95 = invisible bedrock)
		{ 125, 5 },  { 126, 44 },           // wooden double slab / slab
		{ 166, 95 },                        // barrier -> invisible bedrock
		{ 55, 0 }, { 69, 0 }, { 77, 0 }, { 143, 0 }, { 70, 0 }, { 72, 0 }, { 147, 0 }, { 148, 0 },
		{ 93, 0 }, { 94, 0 }, { 149, 0 }, { 150, 0 }, { 131, 0 }, { 132, 0 }, { 90, 0 }, { 119, 0 },
		{ 117, 0 }, { 118, 0 }, { 122, 0 }, { 140, 0 }, { 144, 0 }, { 171, 0 }, { 176, 0 }, { 177, 0 },
		{ 36, 0 }, { 34, 0 },
		{ 75, 50 },  { 76, 50 },            // redstone torches -> torch
		{ 175, 31 },                        // double plants -> tall grass
		{ 113, 85 }, { 188, 85 }, { 189, 85 }, { 190, 85 }, { 191, 85 }, { 192, 85 }, { 139, 85 },
		{ 183, 107 }, { 184, 107 }, { 185, 107 }, { 186, 107 }, { 187, 107 },
		{ 193, 64 }, { 194, 64 }, { 195, 64 }, { 196, 64 }, { 197, 64 },
		{ 167, 96 },                        // iron trapdoor
		{ 130, 54 }, { 146, 54 },           // ender / trapped chest
		{ 123, 89 }, { 124, 89 }, { 169, 89 }, // lamps, sea lantern -> glowstone
		{ 138, 20 },                        // beacon
		{ 179, 24 }, { 180, 128 }, { 181, 44 }, { 182, 44 }, // red sandstone
		{ 151, 44 }, { 178, 44 },           // daylight sensors
		{ 84, 5 }, { 25, 5 },               // jukebox, note block
	};
	for (unsigned int i = 0; i < sizeof(m) / sizeof(m[0]); ++i) {
		int to = m[i].to;
		if (to != 0 && to < 256 && !Tile::tiles[to]) to = 1;
		s_idMap[m[i].from] = (unsigned char)to;
	}
	if (!Tile::tiles[141]) s_idMap[141] = Tile::tiles[59] ? 59 : 0; // carrots
	if (!Tile::tiles[142]) s_idMap[142] = Tile::tiles[59] ? 59 : 0; // potatoes
	s_idMap[0] = 0;
}

/** Maps a 1.8 block state to (id, data) for this engine. */
static inline void mapBlock(int state, int& id, int& data)
{
	int bid = (state >> 4) & 0xFFF, meta = state & 15;
	if (bid >= 256) { id = 1; data = 0; return; }
	id = s_idMap[bid];
	data = meta;
	switch (bid) {
	case 1:  data = 0; break;                          // granite/diorite/andesite
	case 3:  if (meta == 2 && Tile::tiles[243]) { id = 243; data = 0; } else data = 0; break; // podzol
	case 17: case 162: data = meta & 3; break;         // log axis bits
	case 18: case 161: data = meta & 3; break;         // leaf decay bits
	case 95: case 160: data = 0; break;
	case 126: data = 2; break;                         // wooden slab
	case 181: case 182: data = 1 | (meta & 8); break;  // red sandstone slab
	case 175: data = 1; break;
	default: break;
	}
	if (id == 0) data = 0;
}

// ---------------------------------------------------------------------------

std::string EaglerClient::normaliseAddress(const std::string& in)
{
	std::string s;
	for (size_t i = 0; i < in.size(); ++i) if (!isspace((unsigned char)in[i])) s += in[i];
	if (s.empty()) return s;
	if (s.compare(0, 5, "ws://") != 0 && s.compare(0, 6, "wss://") != 0) {
		if (s.compare(0, 8, "https://") == 0) s = "wss://" + s.substr(8);
		else if (s.compare(0, 7, "http://") == 0) s = "ws://" + s.substr(7);
		else s = "wss://" + s;
	}
	size_t scheme = s.find("://") + 3;
	if (s.find('/', scheme) == std::string::npos) s += "/";
	return s;
}

std::string EaglerClient::sanitiseUsername(const std::string& in)
{
	std::string s;
	for (size_t i = 0; i < in.size() && s.size() < 16; ++i) {
		char c = in[i];
		if (isalnum((unsigned char)c) || c == '_') s += c;
	}
	while (s.size() < 3) s += '_';
	return s;
}

EaglerClient::EaglerClient(Minecraft* mc, const std::string& address_, const std::string& username_)
:	mc(mc),
	address(normaliseAddress(address_)),
	username(sanitiseUsername(username_)),
	state(CONNECTING),
	status("Connecting to the server..."),
	ticks(0),
	level(0),
	levelReady(false),
	haveOffset(false),
	offX(0), offZ(0),
	playerEid(0),
	gameMode(0),
	positionKnown(false),
	released(false),
	held(false),
	holdX(0), holdY(0), holdZ(0),
	lastX(0), lastY(0), lastZ(0),
	lastYaw(0), lastPitch(0),
	ticksSinceMove(0),
	lastSelected(-1),
	lastHeldId(-1), lastHeldData(-1),
	digging(false),
	openWindowId(-1),
	actionCounter(0),
	borderX(0), borderZ(0)
{
	initIdMap();
	AppPlatform* p = mc->platform();
	if (!p || !p->wsSupported()) { fail("Joining servers isn't supported on this platform."); return; }
	if (address.empty()) { fail("Enter a server address."); return; }
	if (p->wsOpen(address) < 0) fail("Could not open a connection.");
}

EaglerClient::~EaglerClient()
{
	BiomeTint::setExternalSource(NULL, NULL);
	if (s_active == this) { s_active = NULL; Inventory::swapIntoHotbarHook = NULL; }
	if (mc->platform()) mc->platform()->wsClose(0);
	for (std::map<long long, CachedChunk*>::iterator it = cache.begin(); it != cache.end(); ++it)
		delete it->second;
}

void EaglerClient::fail(const std::string& why)
{
	if (state == FAILED || state == CLOSED) return;
	LOGI("Eaglercraft: %s\n", why.c_str());
	error = why;
	state = FAILED;
	if (mc->platform()) mc->platform()->wsClose(0);
}

void EaglerClient::send(const PacketOut& p)
{
	if (p.buf.empty() || state == FAILED || state == CLOSED) return;
	mc->platform()->wsSend(0, &p.buf[0], (int)p.buf.size());
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------
void EaglerClient::tick()
{
	if (state == FAILED || state == CLOSED) return;
	++ticks;
	AppPlatform* p = mc->platform();
	int ws = p->wsState(0);

	if (state == CONNECTING) {
		if (ws == AppPlatform::WS_OPEN) {
			state = HANDSHAKE;
			status = "Logging in...";
			sendHandshakeVersion();
		} else if (ws == AppPlatform::WS_CLOSED) {
			std::string e = p->wsError(0);
			fail(e.empty() ? "Could not connect to the server." : e);
			return;
		} else if (ticks > 20 * 20) {
			fail("Connection timed out.");
			return;
		}
	}

	// level generation finished? then replay everything that queued up
	if (level && !levelReady && mc->level == (Level*)level && mc->isLevelGenerated() && mc->player) {
		levelReady = true;
		BiomeTint::setExternalSource(&EaglerClient::biomeAt, this);
		s_active = this;
		Inventory::swapIntoHotbarHook = &EaglerClient::swapIntoHotbar;
		applyCachedWindow();
		while (!pendingPlay.empty() && state == PLAY) {
			handlePlay(pendingPlay.front());
			pendingPlay.pop_front();
		}
	}

	std::vector<unsigned char> msg;
	int budget = 400;
	while (budget-- > 0 && state != FAILED && p->wsPoll(0, msg)) {
		if (msg.empty()) continue;
		if (state == HANDSHAKE || state == LOGIN) handleHandshake(msg);
		else if (state == PLAY) {
			if (level && !levelReady) pendingPlay.push_back(msg);
			else handlePlay(msg);
		}
	}

	if (state != FAILED && p->wsState(0) == AppPlatform::WS_CLOSED) {
		// the kick reason (0x40 / 0xFF) can still be queued behind the close
		while (state != FAILED && p->wsPoll(0, msg)) {
			if (msg.empty()) continue;
			if (state == HANDSHAKE || state == LOGIN) handleHandshake(msg);
			else if (state == PLAY && (!level || levelReady)) handlePlay(msg);
			else if (state == PLAY && !msg.empty() && msg[0] == 0x40) { levelReady = true; handlePlay(msg); }
		}
		std::string e = p->wsError(0);
		fail(e.empty() ? "Disconnected from the server." : e);
		return;
	}

	if (state == PLAY && levelReady && mc->player) {
		recentreIfNeeded();
		LocalPlayer* pl = mc->player;
		const bool chunkHere = localChunkReady(Mth::floor(pl->x) >> 4, Mth::floor(pl->z) >> 4);
		held = false;
		if (positionKnown && !released) {
			// after a server teleport: wait at that spot until its terrain arrived
			if (chunkHere) released = true;
			else {
				pl->moveTo((float)(holdX - offX), (float)holdY, (float)(holdZ - offZ), pl->yRot, pl->xRot);
				pl->xd = pl->yd = pl->zd = 0;
				held = true;
			}
		} else if (released && !chunkHere) {
			// walked to the edge of the loaded terrain: stop there instead of
			// falling into the not-yet-received chunk
			pl->setPos(pl->xo, pl->yo, pl->zo);
			pl->xd = pl->yd = pl->zd = 0;
			held = true;
		}
		if (positionKnown) sendMovement();
		syncHeldItem();
	}
}

// ---------------------------------------------------------------------------
// Eaglercraft handshake (protocol v3, Minecraft 47)
// ---------------------------------------------------------------------------
void EaglerClient::sendHandshakeVersion()
{
	PacketOut o;
	o.u8(0x01); o.u8(0x02);          // PROTOCOL_CLIENT_VERSION, v2+ format
	o.u16(1); o.u16(3);              // supported Eagler protocols: 3
	o.u16(1); o.u16(47);             // supported Minecraft protocols: 47 (1.8)
	o.str8("EaglercraftX");
	o.str8("MCPE Extended");
	o.u8(0);                         // no auth
	o.str8(username);
	send(o);
}

void EaglerClient::handleHandshake(const std::vector<unsigned char>& msg)
{
	PacketIn in(&msg[0], msg.size());
	int id = in.u8();
	switch (id) {
	case 0x02: { // server version
		int proto = in.u16(); in.u16();
		int bl = in.u8(); in.skip(bl);
		int vl = in.u8(); in.skip(vl);
		int authMethod = in.u8();
		if (proto != 3) { fail("Unsupported server version."); return; }
		if (authMethod != 0) { fail("This server requires a password login, which isn't supported."); return; }
		PacketOut o;
		o.u8(0x04);                      // request login
		o.str8(username);
		o.str8("default");
		o.u8(0);                         // no password
		send(o);
		state = LOGIN;
		break;
	}
	case 0x03:
		fail("This server doesn't support Eaglercraft 1.8 clients.");
		break;
	case 0x05: { // allow login
		int l = in.u8();
		std::string name((const char*)in.cur(), l > (int)in.remaining() ? in.remaining() : l);
		in.skip(l);
		if (!name.empty()) username = name;
		// profile: our skin (custom pixels, or the Steve/Alex preset)
		PacketOut skin;
		skin.u8(0x07);
		skin.str8("skin_v1");
		const std::vector<unsigned char>& px = PlayerSkin::localEaglerPixels();
		if (PlayerSkin::localIsCustom() && px.size() == 16384) {
			skin.u16(2 + 16384);
			skin.u8(0x02);
			skin.u8(PlayerSkin::localIsSlim() ? 1 : 0);
			skin.raw(&px[0], px.size());
		} else {
			skin.u16(5);
			skin.u8(0x01); skin.i32(PlayerSkin::localIsSlim() ? 1 : 0);
		}
		send(skin);
		PacketOut fin;
		fin.u8(0x08);
		send(fin);
		status = "Joining world...";
		break;
	}
	case 0x06: {
		int l = in.u8();
		std::string why((const char*)in.cur(), l > (int)in.remaining() ? in.remaining() : l);
		fail(why.empty() ? "Login denied." : chatToText(why));
		break;
	}
	case 0x09:
		state = PLAY;
		status = "Downloading terrain...";
		break;
	case 0xFF: {
		in.u8(); // code
		int l = in.u16();
		std::string why((const char*)in.cur(), l > (int)in.remaining() ? in.remaining() : l);
		fail(why.empty() ? "The server closed the connection." : chatToText(why));
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Play state
// ---------------------------------------------------------------------------
void EaglerClient::handlePlay(const std::vector<unsigned char>& msg)
{
	PacketIn in(&msg[0], msg.size());
	int id = in.varint();
	if (!in.ok()) return;

	// everything except these needs a level
	if (!level && id != 0x00 && id != 0x01 && id != 0x05 && id != 0x40 && id != 0x26 && id != 0x21 && id != 0x38)
		return;

	switch (id) {
	case 0x00: { PacketOut o(0x00); o.varint(in.varint()); send(o); break; } // keep alive
	case 0x01: onJoinGame(in); break;
	case 0x02: onChat(in); break;
	case 0x03: {
		in.i64();
		long long t = in.i64();
		level->adventureSettings.doTickTime = t >= 0;
		level->setTime((long)(t < 0 ? -t : t));
		break;
	}
	case 0x04: onEquipment(in); break;
	case 0x05: { // spawn position: good first guess for the window
		int x, y, z; in.position(x, y, z);
		if (!haveOffset) { offX = ((x >> 4) - WINDOW_CHUNKS / 2) * 16; offZ = ((z >> 4) - WINDOW_CHUNKS / 2) * 16; haveOffset = true; }
		break;
	}
	case 0x06: { // health
		float h = in.f32(); in.varint(); in.f32();
		if (mc->player) {
			int hp = (int)ceilf(h);
			if (hp < 0) hp = 0; if (hp > 20) hp = 20;
			if (hp < mc->player->health && hp > 0) { mc->player->hurtTime = mc->player->hurtDuration = 10; }
			mc->player->health = hp;
		}
		break;
	}
	case 0x07: { // respawn
		int dim = in.i32();
		in.u8(); int gm = in.u8() & 7;
		gameMode = gm;
		mc->setIsCreativeMode(gm == 1 || gm == 3);
		if (dim != 0) mc->gui.addMessage("The Nether and the End aren't supported; showing nothing until you're back in the overworld.");
		// drop everything; the server resends chunks and entities
		std::vector<int> ids;
		for (std::map<int, TrackedEntity>::iterator it = entities.begin(); it != entities.end(); ++it) ids.push_back(it->first);
		for (size_t i = 0; i < ids.size(); ++i) removeTracked(ids[i]);
		for (std::map<long long, CachedChunk*>::iterator it = cache.begin(); it != cache.end(); ++it) delete it->second;
		cache.clear();
		applyCachedWindow();
		positionKnown = false;
		break;
	}
	case 0x08: onPositionLook(in); break;
	case 0x09: { int s = in.i8(); if (mc->player && s >= 0 && s < 9) { mc->player->inventory->selectSlot(s); lastSelected = s; } break; }
	case 0x0B: { // animation
		int eid = in.varint(); int anim = in.u8();
		Entity* e = live(eid);
		if (e && e->isMob()) {
			if (anim == 0) ((Mob*)e)->swing();
			else if (anim == 1) e->handleEntityEvent(2);
		}
		break;
	}
	case 0x0C: onSpawnPlayer(in); break;
	case 0x0D: { int item = in.varint(); in.varint(); removeTracked(item); break; } // collect
	case 0x0E: onSpawnObject(in); break;
	case 0x0F: onSpawnMob(in); break;
	case 0x12: onVelocity(in); break;
	case 0x13: onDestroyEntities(in); break;
	case 0x15: onEntityMove(in, true, false); break;
	case 0x16: onEntityMove(in, false, true); break;
	case 0x17: onEntityMove(in, true, true); break;
	case 0x18: onEntityTeleport(in); break;
	case 0x1A: { // entity status
		int eid = in.i32(); int st = in.i8();
		if (Entity* e = live(eid)) e->handleEntityEvent((char)st);
		break;
	}
	case 0x1C: onMetadata(in); break;
	case 0x21: onChunk(in); break;
	case 0x22: onMultiBlockChange(in); break;
	case 0x23: { int x, y, z; in.position(x, y, z); int st = in.varint(); onBlockChange(x, y, z, st); break; }
	case 0x26: onChunkBulk(in); break;
	case 0x27: onExplosion(in); break;
	case 0x2B: { // game state
		int reason = in.u8(); float v = in.f32();
		if (reason == 3) { gameMode = (int)v; mc->setIsCreativeMode(gameMode == 1 || gameMode == 3); }
		break;
	}
	case 0x2D: onOpenWindow(in); break;
	case 0x2E: { int w = in.u8(); if (JavaInventoryScreen::active && JavaInventoryScreen::active->getWindowId() == w && w != 0) mc->setScreen(NULL); break; }
	case 0x32: { // confirm transaction: answer rejected clicks so the server resyncs
		int w = in.i8(); int action = in.i16(); bool ok = in.boolean();
		if (!ok) { PacketOut o(0x0F); o.u8(w); o.u16(action); o.boolean(true); send(o); }
		break;
	}
	case 0x2F: onSetSlot(in); break;
	case 0x30: onWindowItems(in); break;
	case 0x38: onPlayerList(in); break;
	case 0x39: { // abilities
		int flags = in.u8(); in.f32(); in.f32();
		if (mc->player) {
			Abilities& a = mc->player->abilities;
			a.invulnerable = (flags & 1) != 0;
			a.flying = (flags & 2) != 0;
			a.mayfly = (flags & 4) != 0;
			a.instabuild = (flags & 8) != 0;
		}
		break;
	}
	case 0x3F: onPluginMessage(in); break;
	case 0x40: { // disconnect
		std::string why = chatToText(in.str());
		fail(why.empty() ? "Disconnected by the server." : why);
		break;
	}
	case 0x44: onWorldBorder(in); break;
	default: break; // not needed for core gameplay
	}
}

void EaglerClient::onJoinGame(PacketIn& in)
{
	playerEid = in.i32();
	gameMode = in.u8() & 7;
	int dim = in.i8();
	in.u8(); in.u8(); in.str(); in.boolean();
	if (dim != 0) mc->gui.addMessage("The Nether and the End aren't supported.");
	if (!level) createLevel();
	else if (mc->player) mc->player->entityId = playerEid;

	// client settings: view distance 8, chat on, all skin layers
	PacketOut s(0x15);
	s.str("en_US"); s.u8(8); s.u8(0); s.boolean(true); s.u8(0x7F);
	send(s);
	PacketOut b(0x17);
	b.str("MC|Brand");
	b.str("MCPE Extended");
	send(b);
}

void EaglerClient::createLevel()
{
	status = "Building terrain...";
	LevelStorageSource* storageSource = mc->getLevelSource();
	const std::string& levelId = LevelStorageSource::TempLevelId;
	storageSource->deleteLevel(levelId);
	const bool creative = (gameMode == 1 || gameMode == 3);
	level = new MultiPlayerLevel(storageSource->selectLevel(levelId, true), "server",
		LevelSettings(0, creative ? GameType::Creative : GameType::Survival, LevelSettings::WORLD_EMPTY),
		SharedConstants::StorageVersion);
	level->isClientSide = true;
	level->adventureSettings.doTickTime = true;
	level->getWorldBorder().setCenter(0, 0);
	level->getWorldBorder().setSize(WorldBorder::MAX_SIZE);

	LocalPlayer* player = new LocalPlayer(mc, level, mc->user, level->dimension->id, creative);
	player->entityId = playerEid;
	player->moveTo((float)WINDOW_HALF, 100, (float)WINDOW_HALF, 0, 0);
	mc->setLevel(level, "Joining server", player);
	mc->setIsCreativeMode(creative);
}

// ---------------------------------------------------------------------------
// Chat (JSON components -> plain text)
// ---------------------------------------------------------------------------
namespace {
	struct JVal {
		enum Type { NUL, STR, NUM, OBJ, ARR, BOOL } type;
		std::string s;
		std::vector<std::pair<std::string, JVal> > obj;
		std::vector<JVal> arr;
		JVal() : type(NUL) {}
		const JVal* get(const char* k) const {
			for (size_t i = 0; i < obj.size(); ++i) if (obj[i].first == k) return &obj[i].second;
			return 0;
		}
	};
	struct JParser {
		const std::string& t; size_t i; int depth;
		JParser(const std::string& t) : t(t), i(0), depth(0) {}
		void ws() { while (i < t.size() && isspace((unsigned char)t[i])) ++i; }
		std::string str() {
			std::string out; ++i;
			while (i < t.size() && t[i] != '"') {
				char c = t[i++];
				if (c == '\\' && i < t.size()) {
					char e = t[i++];
					if (e == 'n') out += '\n'; else if (e == 't') out += ' ';
					else if (e == 'u' && i + 4 <= t.size()) {
						unsigned cp = (unsigned)strtoul(t.substr(i, 4).c_str(), 0, 16); i += 4;
						if (cp < 0x80) out += (char)cp;
						else if (cp == 0xA7) out += "\xC2\xA7";
						else out += '?';
					} else out += e;
				} else out += c;
			}
			++i;
			return out;
		}
		JVal val() {
			JVal v; ws();
			if (i >= t.size() || ++depth > 64) return v;
			char c = t[i];
			if (c == '"') { v.type = JVal::STR; v.s = str(); }
			else if (c == '{') {
				v.type = JVal::OBJ; ++i;
				for (;;) { ws(); if (i >= t.size()) break; if (t[i] == '}') { ++i; break; }
					if (t[i] == ',') { ++i; continue; }
					if (t[i] != '"') { ++i; continue; }
					std::string k = str(); ws(); if (i < t.size() && t[i] == ':') ++i;
					v.obj.push_back(std::make_pair(k, val())); }
			} else if (c == '[') {
				v.type = JVal::ARR; ++i;
				for (;;) { ws(); if (i >= t.size()) break; if (t[i] == ']') { ++i; break; }
					if (t[i] == ',') { ++i; continue; }
					v.arr.push_back(val()); }
			} else {
				size_t st = i;
				while (i < t.size() && t[i] != ',' && t[i] != '}' && t[i] != ']' && !isspace((unsigned char)t[i])) ++i;
				v.s = t.substr(st, i - st);
				v.type = (v.s == "true" || v.s == "false") ? JVal::BOOL : (v.s == "null" ? JVal::NUL : JVal::NUM);
			}
			--depth;
			return v;
		}
	};
	std::string flatten(const JVal& v);
	std::string translate(const std::string& key, const std::vector<std::string>& args) {
		const char* fmt = 0;
		if (key == "chat.type.text") fmt = "<%s> %s";
		else if (key == "chat.type.announcement") fmt = "[%s] %s";
		else if (key == "chat.type.emote") fmt = "* %s %s";
		else if (key == "chat.type.admin") fmt = "[%s: %s]";
		else if (key == "multiplayer.player.joined") fmt = "%s joined the game";
		else if (key == "multiplayer.player.left") fmt = "%s left the game";
		else if (key == "commands.generic.notFound") fmt = "Unknown command. Try /help for a list of commands";
		std::string out;
		if (fmt) {
			size_t a = 0;
			for (const char* p = fmt; *p; ++p) {
				if (p[0] == '%' && p[1] == 's') { if (a < args.size()) out += args[a++]; ++p; }
				else out += *p;
			}
			return out;
		}
		out = key;
		for (size_t i = 0; i < args.size(); ++i) out += " " + args[i];
		return out;
	}
	std::string flatten(const JVal& v) {
		if (v.type == JVal::STR || v.type == JVal::NUM) return v.s;
		if (v.type == JVal::ARR) { std::string s; for (size_t i = 0; i < v.arr.size(); ++i) s += flatten(v.arr[i]); return s; }
		if (v.type != JVal::OBJ) return "";
		std::string s;
		if (const JVal* t = v.get("text")) s += flatten(*t);
		if (const JVal* tr = v.get("translate")) {
			std::vector<std::string> args;
			if (const JVal* w = v.get("with")) for (size_t i = 0; i < w->arr.size(); ++i) args.push_back(flatten(w->arr[i]));
			s += translate(tr->s, args);
		}
		if (const JVal* e = v.get("extra")) s += flatten(*e);
		return s;
	}
	std::string stripCodes(const std::string& in) {
		std::string out;
		for (size_t i = 0; i < in.size(); ++i) {
			unsigned char c = (unsigned char)in[i];
			if (c == 0xC2 && i + 1 < in.size() && (unsigned char)in[i + 1] == 0xA7) { i += 2; continue; } // section sign + code
			if (c == 0xA7) { ++i; continue; }
			if (c >= 0x80) { // other UTF-8: skip continuation bytes, show '?'
				while (i + 1 < in.size() && ((unsigned char)in[i + 1] & 0xC0) == 0x80) ++i;
				out += '?';
				continue;
			}
			if (c >= 32 || c == '\n') out += (char)c;
		}
		return out;
	}
}

std::string EaglerClient::chatToText(const std::string& json)
{
	std::string s;
	if (!json.empty() && (json[0] == '{' || json[0] == '[' || json[0] == '"')) {
		JParser p(json);
		s = flatten(p.val());
	} else s = json;
	return stripCodes(s);
}

void EaglerClient::onChat(PacketIn& in)
{
	std::string text = chatToText(in.str());
	int pos = in.u8();
	if (pos == 2 || text.empty()) return; // action bar
	size_t st = 0;
	while (st <= text.size()) {
		size_t nl = text.find('\n', st);
		std::string line = text.substr(st, nl == std::string::npos ? std::string::npos : nl - st);
		if (!line.empty()) mc->gui.addMessage(line);
		if (nl == std::string::npos) break;
		st = nl + 1;
	}
}

void EaglerClient::sendChat(const std::string& message)
{
	if (state != PLAY) return;
	PacketOut o(0x01);
	o.str(message.substr(0, 100));
	send(o);
}

void EaglerClient::requestRespawn()
{
	PacketOut o(0x16);
	o.varint(0);
	send(o);
}

// ---------------------------------------------------------------------------
// Player position
// ---------------------------------------------------------------------------
void EaglerClient::onPositionLook(PacketIn& in)
{
	double x = in.f64(), y = in.f64(), z = in.f64();
	float yaw = in.f32(), pitch = in.f32();
	int flags = in.u8();
	LocalPlayer* pl = mc->player;
	double curX = pl ? pl->x + offX : 0, curY = pl ? pl->bb.y0 : 0, curZ = pl ? pl->z + offZ : 0;
	if (flags & 1) x += curX;
	if (flags & 2) y += curY;
	if (flags & 4) z += curZ;
	if (pl) { if (flags & 8) yaw += pl->yRot; if (flags & 16) pitch += pl->xRot; }
	// dev/testing: force the view direction
	if (getenv("MCPE_LOOKDOWN")) pitch = 80;
	if (const char* v = getenv("MCPE_YAW")) yaw = (float)atof(v);
	if (const char* v = getenv("MCPE_PITCH")) pitch = (float)atof(v);

	if (!haveOffset) {
		offX = (((int)floor(x) >> 4) - WINDOW_CHUNKS / 2) * 16;
		offZ = (((int)floor(z) >> 4) - WINDOW_CHUNKS / 2) * 16;
		haveOffset = true;
		if (levelReady) applyCachedWindow();
	}
	holdX = x; holdY = y; holdZ = z;
	positionKnown = true;
	released = false;
	if (pl) {
		pl->moveTo((float)(x - offX), (float)y, (float)(z - offZ), yaw, pitch);
		pl->xd = pl->yd = pl->zd = 0;
		pl->fallDistance = 0;
		recentreIfNeeded();
	}
	lastX = x; lastY = y; lastZ = z; lastYaw = yaw; lastPitch = pitch;
	// confirm (1.8 clients answer a teleport with a full position packet)
	PacketOut o(0x06);
	o.f64(x); o.f64(y); o.f64(z); o.f32(yaw); o.f32(pitch); o.boolean(false);
	send(o);
	if (status != "") status = "";
}

void EaglerClient::sendMovement()
{
	LocalPlayer* pl = mc->player;
	double x = pl->x + offX, y = pl->bb.y0, z = pl->z + offZ;
	float yaw = pl->yRot, pitch = pl->xRot;
	bool moved = fabs(x - lastX) > 0.03 || fabs(y - lastY) > 0.03 || fabs(z - lastZ) > 0.03 || ++ticksSinceMove >= 20;
	bool rotated = yaw != lastYaw || pitch != lastPitch;
	// held while terrain loads: claim ground so anti-fly checks don't kick us
	const bool onGround = held ? true : pl->onGround;
	if (moved && rotated) {
		PacketOut o(0x06); o.f64(x); o.f64(y); o.f64(z); o.f32(yaw); o.f32(pitch); o.boolean(onGround); send(o);
	} else if (moved) {
		PacketOut o(0x04); o.f64(x); o.f64(y); o.f64(z); o.boolean(onGround); send(o);
	} else if (rotated) {
		PacketOut o(0x05); o.f32(yaw); o.f32(pitch); o.boolean(onGround); send(o);
	} else {
		PacketOut o(0x03); o.boolean(onGround); send(o);
	}
	if (moved) { lastX = x; lastY = y; lastZ = z; ticksSinceMove = 0; }
	if (rotated) { lastYaw = yaw; lastPitch = pitch; }
}

void EaglerClient::syncHeldItem()
{
	Inventory* inv = mc->player->inventory;
	if (inv->selected != lastSelected) {
		lastSelected = inv->selected;
		PacketOut o(0x09); o.u16(lastSelected); send(o);
	}
	if (gameMode == 1) {
		// creative: tell the server what we're holding so it can be placed
		ItemInstance* it = inv->getSelected();
		int id = (it && it->count > 0) ? it->id : 0, data = (it && it->count > 0) ? it->getAuxValue() : 0;
		if (id != lastHeldId || data != lastHeldData) {
			lastHeldId = id; lastHeldData = data;
			PacketOut o(0x10);
			o.u16(36 + inv->selected);
			ItemInstance stack(id, it && id ? it->getMaxStackSize() : 0, data);
			writeSlot(o, id ? &stack : 0);
			send(o);
		}
	}
}

// ---------------------------------------------------------------------------
// World window
// ---------------------------------------------------------------------------
bool EaglerClient::chunkInWindow(int scx, int scz) const
{
	int lcx = scx - (offX >> 4), lcz = scz - (offZ >> 4);
	return lcx >= 0 && lcz >= 0 && lcx < WINDOW_CHUNKS && lcz < WINDOW_CHUNKS;
}

bool EaglerClient::localChunkReady(int lcx, int lcz) const
{
	if (lcx < 0 || lcz < 0 || lcx >= WINDOW_CHUNKS || lcz >= WINDOW_CHUNKS) return false;
	return cache.find(key(lcx + (offX >> 4), lcz + (offZ >> 4))) != cache.end();
}

void EaglerClient::applyChunkToLevel(int scx, int scz, const CachedChunk* c)
{
	if (!levelReady || !chunkInWindow(scx, scz)) return;
	int lcx = scx - (offX >> 4), lcz = scz - (offZ >> 4);
	LevelChunk* lc = level->getChunk(lcx, lcz);
	if (!lc || !lc->getBlockData()) return;
	if (c) {
		memcpy(lc->getBlockData(), c->blocks, sizeof(c->blocks));
		memcpy(lc->data.data, c->meta, sizeof(c->meta));
		memcpy(lc->skyLight.data, c->sky, sizeof(c->sky));
		memcpy(lc->blockLight.data, c->light, sizeof(c->light));
	} else {
		memset(lc->getBlockData(), 0, 16 * 16 * HEIGHT);
		memset(lc->data.data, 0, 16 * 16 * HEIGHT / 2);
		memset(lc->skyLight.data, 0xFF, 16 * 16 * HEIGHT / 2);
		memset(lc->blockLight.data, 0, 16 * 16 * HEIGHT / 2);
	}
	lc->recalcHeightmapOnly();
	lc->unsaved = false;
	BiomeTint::invalidateBlock(lcx, lcz);
	level->setTilesDirty(lcx * 16, 0, lcz * 16, lcx * 16 + 15, HEIGHT - 1, lcz * 16 + 15);
}

void EaglerClient::applyCachedWindow()
{
	if (!levelReady) return;
	for (int lcx = 0; lcx < WINDOW_CHUNKS; ++lcx)
	for (int lcz = 0; lcz < WINDOW_CHUNKS; ++lcz) {
		int scx = lcx + (offX >> 4), scz = lcz + (offZ >> 4);
		std::map<long long, CachedChunk*>::iterator it = cache.find(key(scx, scz));
		applyChunkToLevel(scx, scz, it != cache.end() ? it->second : 0);
	}
	WorldBorder& wb = level->getWorldBorder();
	if (wb.getSize() < WorldBorder::MAX_SIZE * 0.5) wb.setCenter(borderX - offX, borderZ - offZ);
}

void EaglerClient::recentreIfNeeded()
{
	LocalPlayer* pl = mc->player;
	if (!pl || !haveOffset) return;
	if (fabs(pl->x - WINDOW_HALF) < RECENTRE_DIST && fabs(pl->z - WINDOW_HALF) < RECENTRE_DIST) return;

	double sx = pl->x + offX, sz = pl->z + offZ;
	int nx = (((int)floor(sx) >> 4) - WINDOW_CHUNKS / 2) * 16;
	int nz = (((int)floor(sz) >> 4) - WINDOW_CHUNKS / 2) * 16;
	float dx = (float)(nx - offX), dz = (float)(nz - offZ);
	offX = nx; offZ = nz;

	// shift every entity (including us) into the new window
	for (size_t i = 0; i < level->entities.size(); ++i) {
		Entity* e = level->entities[i];
		e->x -= dx; e->z -= dz;
		e->xo -= dx; e->zo -= dz;
		e->xOld -= dx; e->zOld -= dz;
		e->setPos(e->x, e->y, e->z);
		if (e->isMob()) ((Mob*)e)->shiftLerpTarget(-dx, -dz);
	}
	BiomeTint::clearCache();
	applyCachedWindow();
	if (mc->levelRenderer) mc->levelRenderer->allChanged();
}

// ---------------------------------------------------------------------------
// Chunks
// ---------------------------------------------------------------------------
void EaglerClient::readChunkColumn(PacketIn& in, int cx, int cz, int mask, bool groundUp, bool skylight)
{
	long long k = key(cx, cz);
	if (groundUp && mask == 0) { // unload
		std::map<long long, CachedChunk*>::iterator it = cache.find(k);
		if (it != cache.end()) { delete it->second; cache.erase(it); }
		applyChunkToLevel(cx, cz, 0);
		return;
	}
	std::map<long long, CachedChunk*>::iterator it = cache.find(k);
	CachedChunk* c;
	if (it == cache.end()) {
		c = new CachedChunk();
		memset(c->blocks, 0, sizeof(c->blocks));
		memset(c->meta, 0, sizeof(c->meta));
		memset(c->sky, 0xFF, sizeof(c->sky));
		memset(c->light, 0, sizeof(c->light));
		memset(c->biomes, 1, sizeof(c->biomes));
		cache[k] = c;
	} else c = it->second;

	int sections[16]; int ns = 0;
	for (int s = 0; s < 16; ++s) if (mask & (1 << s)) sections[ns++] = s;

	// block states: 4096 little-endian shorts per section, index (y << 8) | (z << 4) | x
	for (int i = 0; i < ns; ++i) {
		const int s = sections[i];
		if (in.remaining() < 8192) { in.skip(in.remaining()); return; }
		const unsigned char* d = in.cur();
		if (s * 16 < HEIGHT) {
			for (int ly = 0; ly < 16; ++ly) {
				const int y = s * 16 + ly;
				for (int z = 0; z < 16; ++z)
				for (int x = 0; x < 16; ++x) {
					const int si = ((ly << 8) | (z << 4) | x) * 2;
					const int st = d[si] | (d[si + 1] << 8);
					int id, data;
					mapBlock(st, id, data);
					const int li = (x << 11) | (z << 7) | y;
					c->blocks[li] = (unsigned char)id;
					unsigned char& m = c->meta[li >> 1];
					m = (li & 1) ? (unsigned char)((m & 0x0F) | (data << 4)) : (unsigned char)((m & 0xF0) | (data & 15));
				}
			}
		}
		in.skip(8192);
	}
	// light: 2048 bytes (nibbles, index (y << 8) | (z << 4) | x) per section
	for (int pass = 0; pass < (skylight ? 2 : 1); ++pass) {
		unsigned char* dst = pass == 0 ? c->light : c->sky;
		for (int i = 0; i < ns; ++i) {
			const int s = sections[i];
			if (in.remaining() < 2048) { in.skip(in.remaining()); return; }
			const unsigned char* d = in.cur();
			if (s * 16 < HEIGHT) {
				for (int ly = 0; ly < 16; ++ly) {
					const int y = s * 16 + ly;
					for (int z = 0; z < 16; ++z)
					for (int x = 0; x < 16; ++x) {
						const int si = (ly << 8) | (z << 4) | x;
						const int v = (si & 1) ? (d[si >> 1] >> 4) : (d[si >> 1] & 15);
						const int li = (x << 11) | (z << 7) | y;
						unsigned char& m = dst[li >> 1];
						m = (li & 1) ? (unsigned char)((m & 0x0F) | (v << 4)) : (unsigned char)((m & 0xF0) | v);
					}
				}
			}
			in.skip(2048);
		}
	}
	if (groundUp && in.remaining() >= 256) { in.bytes(c->biomes, 256); } // index z * 16 + x
	if (!haveOffset) { offX = (cx - WINDOW_CHUNKS / 2) * 16; offZ = (cz - WINDOW_CHUNKS / 2) * 16; haveOffset = true; }
	applyChunkToLevel(cx, cz, c);
}

void EaglerClient::onChunk(PacketIn& in)
{
	int cx = in.i32(), cz = in.i32();
	bool groundUp = in.boolean();
	int mask = in.u16();
	int size = in.varint();
	if (!in.ok() || size < 0 || (size_t)size > in.remaining()) return;
	PacketIn data(in.cur(), size);
	readChunkColumn(data, cx, cz, mask, groundUp, true);
}

void EaglerClient::onChunkBulk(PacketIn& in)
{
	bool sky = in.boolean();
	int n = in.varint();
	if (n < 0 || n > 1024) return;
	std::vector<int> xs(n), zs(n), masks(n);
	for (int i = 0; i < n; ++i) { xs[i] = in.i32(); zs[i] = in.i32(); masks[i] = in.u16(); }
	for (int i = 0; i < n && in.ok(); ++i)
		readChunkColumn(in, xs[i], zs[i], masks[i], true, sky);
}

void EaglerClient::onBlockChange(int x, int y, int z, int st)
{
	if (y < 0 || y >= HEIGHT) return;
	int id, data;
	mapBlock(st, id, data);
	std::map<long long, CachedChunk*>::iterator it = cache.find(key(x >> 4, z >> 4));
	if (it != cache.end()) {
		CachedChunk* c = it->second;
		const int li = ((x & 15) << 11) | ((z & 15) << 7) | y;
		c->blocks[li] = (unsigned char)id;
		unsigned char& m = c->meta[li >> 1];
		m = (li & 1) ? (unsigned char)((m & 0x0F) | (data << 4)) : (unsigned char)((m & 0xF0) | data);
	}
	if (level && levelReady && chunkInWindow(x >> 4, z >> 4))
		level->setTileAndData(x - offX, y, z - offZ, id, data);
}

void EaglerClient::onMultiBlockChange(PacketIn& in)
{
	int cx = in.i32(), cz = in.i32();
	int n = in.varint();
	for (int i = 0; i < n && in.ok(); ++i) {
		int xz = in.u8(); int y = in.u8(); int st = in.varint();
		onBlockChange(cx * 16 + (xz >> 4), y, cz * 16 + (xz & 15), st);
	}
}

void EaglerClient::onExplosion(PacketIn& in)
{
	float x = in.f32(), y = in.f32(), z = in.f32();
	in.f32();
	int n = in.i32();
	int bx = (int)floorf(x), by = (int)floorf(y), bz = (int)floorf(z);
	for (int i = 0; i < n && in.ok(); ++i) {
		int dx = in.i8(), dy = in.i8(), dz = in.i8();
		onBlockChange(bx + dx, by + dy, bz + dz, 0);
	}
	float mx = in.f32(), my = in.f32(), mz = in.f32();
	if (mc->player && in.ok()) { mc->player->xd += mx; mc->player->yd += my; mc->player->zd += mz; }
	if (level) level->playSound(x - offX, y, z - offZ, "random.explode", 4, 1);
}

void EaglerClient::onWorldBorder(PacketIn& in)
{
	WorldBorder& wb = level->getWorldBorder();
	int action = in.varint();
	switch (action) {
	case 0: wb.setSize(in.f64()); break;
	case 1: { double a = in.f64(), b = in.f64(); long long ms = in.varlong(); wb.lerpSizeBetween(a, b, ms); break; }
	case 2: borderX = in.f64(); borderZ = in.f64(); wb.setCenter(borderX - offX, borderZ - offZ); break;
	case 3: {
		borderX = in.f64(); borderZ = in.f64();
		double a = in.f64(), b = in.f64(); long long ms = in.varlong();
		in.varint();
		int warnTime = in.varint(), warnBlocks = in.varint();
		wb.setCenter(borderX - offX, borderZ - offZ);
		if (ms > 0) wb.lerpSizeBetween(a, b, ms); else wb.setSize(b);
		wb.setWarningTime(warnTime); wb.setWarningBlocks(warnBlocks);
		break;
	}
	case 4: wb.setWarningTime(in.varint()); break;
	case 5: wb.setWarningBlocks(in.varint()); break;
	}
}

// ---------------------------------------------------------------------------
// Inventory
// ---------------------------------------------------------------------------
static void skipNbtPayload(PacketIn& in, int type, int depth)
{
	if (depth > 32 || !in.ok()) { in.skip(in.remaining()); return; }
	switch (type) {
	case 1: in.skip(1); break;
	case 2: in.skip(2); break;
	case 3: in.skip(4); break;
	case 4: in.skip(8); break;
	case 5: in.skip(4); break;
	case 6: in.skip(8); break;
	case 7: { int n = in.i32(); if (n > 0) in.skip(n); break; }
	case 8: { int n = in.u16(); in.skip(n); break; }
	case 9: { int t = in.u8(); int n = in.i32(); for (int i = 0; i < n && in.ok(); ++i) skipNbtPayload(in, t, depth + 1); break; }
	case 10: for (;;) { int t = in.u8(); if (t == 0 || !in.ok()) break; int nl = in.u16(); in.skip(nl); skipNbtPayload(in, t, depth + 1); } break;
	case 11: { int n = in.i32(); if (n > 0) in.skip((size_t)n * 4); break; }
	default: in.skip(in.remaining()); break;
	}
}

bool EaglerClient::readSlot(PacketIn& in, ItemInstance& out)
{
	int id = in.i16();
	if (id < 0) { out = ItemInstance(); return false; }
	int count = in.i8();
	int damage = in.i16();
	int nbt = in.u8();
	if (nbt != 0) { int nl = in.u16(); in.skip(nl); skipNbtPayload(in, nbt, 0); }
	if (id < 256) {
		int mid, mdata;
		mapBlock((id << 4) | (damage & 15), mid, mdata);
		if (mid == 0) { out = ItemInstance(); return false; }
		if (id != 17 && id != 162 && id != 18 && id != 161 && id != 3) mdata = damage;
		id = mid; damage = mdata;
	}
	if (id >= 512 || !Item::items[id]) { out = ItemInstance(); return false; }
	out = ItemInstance(id, count, damage);
	return true;
}

void EaglerClient::writeSlot(PacketOut& o, const ItemInstance* item)
{
	if (!item || item->isNull() || item->count <= 0) { o.u16(0xFFFF); return; }
	int id = item->id, data = item->getAuxValue();
	if (id == 243) { id = 3; data = 2; } // podzol
	o.u16(id);
	o.u8(item->count);
	o.u16(data);
	o.u8(0); // no NBT
}

void EaglerClient::applyWindowSlot(int window, int slot, const ItemInstance& it, bool has)
{
	if (!mc->player || mc->isCreativeMode()) return;
	JavaInventoryScreen* js = JavaInventoryScreen::active;
	if (window == -1 && slot == -1) { if (js) js->serverSetSlot(-1, has ? it : ItemInstance()); return; }
	Inventory* inv = mc->player->inventory;
	int peSlot = -1;
	if (window == 0) {
		if (slot >= 0 && slot <= 4) { if (js && js->getWindowId() == 0) js->serverSetSlot(slot, has ? it : ItemInstance()); return; }
		if (slot >= 5 && slot <= 8) { mc->player->setArmor(slot - 5, has ? &it : NULL); return; }
		if (slot >= 9 && slot <= 44) peSlot = slot;
	} else if (window == openWindowId && window != 0) {
		// crafting table: result 0, grid 1-9, storage 10-36, hotbar 37-45
		if (slot >= 0 && slot <= 9) { if (js && js->getWindowId() == window) js->serverSetSlot(slot, has ? it : ItemInstance()); return; }
		if (slot >= 10 && slot <= 45) peSlot = slot - 1;
	}
	if (peSlot < 9 || peSlot > 44) return;
	if (has) { ItemInstance copy(it); inv->replaceSlot(peSlot, &copy); } else inv->clearSlot(peSlot);
	JavaInventoryScreen::fixHotbarLinks(inv);
}

void EaglerClient::onWindowItems(PacketIn& in)
{
	int window = in.u8();
	int n = in.i16();
	for (int s = 0; s < n && in.ok(); ++s) {
		ItemInstance it;
		bool has = readSlot(in, it);
		applyWindowSlot(window, s, it, has);
	}
}

void EaglerClient::onSetSlot(PacketIn& in)
{
	int window = in.i8();
	int slot = in.i16();
	ItemInstance it;
	bool has = readSlot(in, it);
	applyWindowSlot(window, slot, it, has);
}

void EaglerClient::onOpenWindow(PacketIn& in)
{
	int id = in.u8();
	std::string type = in.str();
	in.str(); in.u8();
	if (type == "minecraft:crafting_table") {
		openWindowId = id;
		mc->setScreen(new JavaInventoryScreen(true, id));
	} else {
		// chests, furnaces, ... aren't supported yet: close it again so the
		// server doesn't think it's still open
		PacketOut o(0x0D); o.u8(id); send(o);
		mc->gui.addMessage("This container isn't supported on servers yet.");
	}
}

void EaglerClient::sendWindowClick(int window, int slot, int button, int mode, const ItemInstance& clicked)
{
	if (!isPlaying()) return;
	PacketOut o(0x0E);
	o.u8(window);
	o.u16(slot);
	o.u8(button);
	o.u16(++actionCounter & 0x7FFF);
	o.u8(mode);
	writeSlot(o, clicked.isNull() ? 0 : &clicked);
	send(o);
}

void EaglerClient::sendCloseWindow(int window)
{
	if (!isPlaying()) return;
	PacketOut o(0x0D); o.u8(window); send(o);
	if (window == openWindowId) openWindowId = -1;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------
void EaglerClient::skipMetadata(PacketIn& in)
{
	for (int guard = 0; guard < 64 && in.ok(); ++guard) {
		int b = in.u8();
		if (b == 0x7F) return;
		switch (b >> 5) {
		case 0: in.skip(1); break;
		case 1: in.skip(2); break;
		case 2: in.skip(4); break;
		case 3: in.skip(4); break;
		case 4: in.str(); break;
		case 5: { ItemInstance tmp; readSlot(in, tmp); break; }
		case 6: in.skip(12); break;
		case 7: in.skip(12); break;
		}
	}
}

void EaglerClient::placeEntity(TrackedEntity& t, bool snap)
{
	Entity* e = live(t.id);
	if (!e) return;
	float x = t.fx / 32.0f - offX, y = t.fy / 32.0f, z = t.fz / 32.0f - offZ;
	if (snap) e->moveTo(x, y, z, t.yaw, t.pitch);
	else e->lerpTo(x, y, z, t.yaw, t.pitch, 3);
}

void EaglerClient::removeTracked(int id)
{
	std::map<int, TrackedEntity>::iterator it = entities.find(id);
	if (it == entities.end()) return;
	Entity* e = live(id);
	entities.erase(it);
	if (e && level) {
		if (e->isPlayer()) ((Player*)e)->reallyRemoveIfPlayer = true;
		level->removeEntity(e);
	}
}

void EaglerClient::onSpawnPlayer(PacketIn& in)
{
	int eid = in.varint();
	char uuid[16]; in.bytes(uuid, 16);
	TrackedEntity t;
	t.fx = in.i32(); t.fy = in.i32(); t.fz = in.i32();
	t.yaw = in.i8() * 360.0f / 256.0f; t.pitch = in.i8() * 360.0f / 256.0f;
	int item = in.i16();
	skipMetadata(in);
	if (!in.ok() || eid == playerEid) return;
	removeTracked(eid);

	RemotePlayer* p = new RemotePlayer(level, mc->isCreativeMode());
	mc->gameMode->initAbilities(p->abilities);
	std::map<std::string, std::string>::iterator nit = playerNames.find(std::string(uuid, 16));
	p->name = nit != playerNames.end() ? nit->second : std::string("Player");
	if (item > 0 && item < 512 && Item::items[item]) {
		ItemInstance held(item, 1, 0);
		p->inventory->replaceSlot(Inventory::MAX_SELECTION_SIZE, &held);
		p->inventory->moveToSelectedSlot(Inventory::MAX_SELECTION_SIZE, true);
	}
	t.id = eid;
	t.isPlayer = true;
	p->moveTo(t.fx / 32.0f - offX, t.fy / 32.0f, t.fz / 32.0f - offZ, t.yaw, t.pitch);
	level->putEntity(eid, p);
	entities[eid] = t;

	LOGI("Eaglercraft: player %s appeared\n", p->name.c_str());
	// skin: ask the server for it (Eaglercraft skin channel)
	std::string skinName = "skin:eag:" + std::string(uuid, 16);
	p->setTextureName(skinName);
	if (requestedSkins.insert(skinName).second) {
		PacketOut o(0x17);
		o.str("EAG|Skins-1.8");
		o.u8(0x03);
		o.raw(uuid, 16);
		send(o);
	}
}

void EaglerClient::onPluginMessage(PacketIn& in)
{
	std::string channel = in.str();
	if (channel != "EAG|Skins-1.8") return;
	int type = in.u8();
	char uuid[16]; in.bytes(uuid, 16);
	if (!in.ok()) return;
	const std::string name = "skin:eag:" + std::string(uuid, 16);
	std::vector<unsigned char> rgba;
	bool slim = false;
	if (type == 0x04) {                 // preset skin: odd ids are the slim (Alex) variants
		int preset = in.i32() & 0x7FFFFFFF;
		slim = (preset & 1) != 0;
		if (!PlayerSkin::loadImage64(mc, slim ? "mob/alex.png" : "mob/char.png", true, rgba)) return;
	} else if (type == 0x05) {          // custom: model byte + 64x64 [A,B,G,R]
		int model = in.u8();
		slim = (model & 0x7F) == 1;
		if (in.remaining() < 16384) return;
		const unsigned char* d = in.cur();
		rgba.resize(16384);
		for (int i = 0; i < 4096; ++i) {
			rgba[i * 4 + 0] = d[i * 4 + 3];
			rgba[i * 4 + 1] = d[i * 4 + 2];
			rgba[i * 4 + 2] = d[i * 4 + 1];
			rgba[i * 4 + 3] = d[i * 4 + 0];
		}
	} else return;
	PlayerSkin::set(mc, name, &rgba[0], slim);
	LOGI("Eaglercraft: received %s skin (%s)\n", type == 0x04 ? "preset" : "custom", slim ? "slim" : "classic");
}

static int mobTypeFor18(int t)
{
	switch (t) {
	case 50: return MobTypes::Creeper;
	case 51: return MobTypes::Skeleton;
	case 52: case 59: return MobTypes::Spider;
	case 54: return MobTypes::Zombie;
	case 57: return MobTypes::PigZombie;
	case 90: return MobTypes::Pig;
	case 91: return MobTypes::Sheep;
	case 92: case 96: return MobTypes::Cow;
	case 93: return MobTypes::Chicken;
	default: return 0;
	}
}

void EaglerClient::onSpawnMob(PacketIn& in)
{
	int eid = in.varint();
	int type = in.u8();
	TrackedEntity t;
	t.fx = in.i32(); t.fy = in.i32(); t.fz = in.i32();
	t.yaw = in.i8() * 360.0f / 256.0f; t.pitch = in.i8() * 360.0f / 256.0f;
	in.i8(); in.i16(); in.i16(); in.i16();
	skipMetadata(in);
	int pe = mobTypeFor18(type);
	if (!pe || !in.ok()) return;
	removeTracked(eid);
	Mob* mob = MobFactory::CreateMob(pe, level);
	if (!mob) return;
	t.id = eid;
	mob->moveTo(t.fx / 32.0f - offX, t.fy / 32.0f, t.fz / 32.0f - offZ, t.yaw, t.pitch);
	level->putEntity(eid, mob);
	entities[eid] = t;
}

void EaglerClient::onSpawnObject(PacketIn& in)
{
	int eid = in.varint();
	int type = in.i8();
	TrackedEntity t;
	t.fx = in.i32(); t.fy = in.i32(); t.fz = in.i32();
	t.pitch = in.i8() * 360.0f / 256.0f; t.yaw = in.i8() * 360.0f / 256.0f;
	int data = in.i32();
	if (data != 0) { in.i16(); in.i16(); in.i16(); }
	if (type != 2) return; // only dropped items; the item arrives with the metadata
	t.pendingItemType = 2;
	t.id = eid;
	entities[eid] = t;
}

void EaglerClient::onMetadata(PacketIn& in)
{
	int eid = in.varint();
	std::map<int, TrackedEntity>::iterator it = entities.find(eid);
	if (it == entities.end() || it->second.pendingItemType != 2) return;
	for (int guard = 0; guard < 64 && in.ok(); ++guard) {
		int b = in.u8();
		if (b == 0x7F) return;
		int type = b >> 5, index = b & 31;
		if (type == 5) {
			ItemInstance item;
			bool has = readSlot(in, item);
			if (index == 10 && has) {
				TrackedEntity& t = it->second;
				t.pendingItemType = 0;
				ItemEntity* ie = new ItemEntity(level, t.fx / 32.0f - offX, t.fy / 32.0f, t.fz / 32.0f - offZ, item);
				ie->xd = ie->yd = ie->zd = 0;
				t.id = eid;
				level->putEntity(eid, ie);
				return;
			}
			continue;
		}
		switch (type) {
		case 0: in.skip(1); break; case 1: in.skip(2); break; case 2: case 3: in.skip(4); break;
		case 4: in.str(); break; case 6: case 7: in.skip(12); break;
		}
	}
}

void EaglerClient::onDestroyEntities(PacketIn& in)
{
	int n = in.varint();
	for (int i = 0; i < n && in.ok(); ++i) removeTracked(in.varint());
}

void EaglerClient::onEntityMove(PacketIn& in, bool move, bool look)
{
	int eid = in.varint();
	int dx = 0, dy = 0, dz = 0;
	if (move) { dx = in.i8(); dy = in.i8(); dz = in.i8(); }
	int yaw = 0, pitch = 0;
	if (look) { yaw = in.i8(); pitch = in.i8(); }
	std::map<int, TrackedEntity>::iterator it = entities.find(eid);
	if (it == entities.end() || !in.ok()) return;
	TrackedEntity& t = it->second;
	t.fx += dx; t.fy += dy; t.fz += dz;
	if (look) { t.yaw = yaw * 360.0f / 256.0f; t.pitch = pitch * 360.0f / 256.0f; }
	placeEntity(t, false);
}

void EaglerClient::onEntityTeleport(PacketIn& in)
{
	int eid = in.varint();
	int x = in.i32(), y = in.i32(), z = in.i32();
	int yaw = in.i8(), pitch = in.i8();
	std::map<int, TrackedEntity>::iterator it = entities.find(eid);
	if (it == entities.end() || !in.ok()) return;
	TrackedEntity& t = it->second;
	t.fx = x; t.fy = y; t.fz = z;
	t.yaw = yaw * 360.0f / 256.0f; t.pitch = pitch * 360.0f / 256.0f;
	placeEntity(t, false);
}

void EaglerClient::onVelocity(PacketIn& in)
{
	int eid = in.varint();
	float vx = in.i16() / 8000.0f, vy = in.i16() / 8000.0f, vz = in.i16() / 8000.0f;
	if (eid == playerEid && mc->player) { mc->player->xd = vx; mc->player->yd = vy; mc->player->zd = vz; return; }
	std::map<int, TrackedEntity>::iterator it = entities.find(eid);
	if (it != entities.end()) if (Entity* e = live(eid)) e->lerpMotion(vx, vy, vz);
}

void EaglerClient::onEquipment(PacketIn& in)
{
	int eid = in.varint();
	int slot = in.i16();
	ItemInstance item;
	bool has = readSlot(in, item);
	if (slot != 0) return;
	std::map<int, TrackedEntity>::iterator it = entities.find(eid);
	Entity* pe = it != entities.end() && it->second.isPlayer ? live(eid) : 0;
	if (!pe) return;
	Player* p = (Player*)pe;
	if (has) p->inventory->replaceSlot(Inventory::MAX_SELECTION_SIZE, &item);
	else p->inventory->clearSlot(Inventory::MAX_SELECTION_SIZE);
	p->inventory->moveToSelectedSlot(Inventory::MAX_SELECTION_SIZE, true);
}

void EaglerClient::onPlayerList(PacketIn& in)
{
	int action = in.varint();
	int n = in.varint();
	for (int i = 0; i < n && in.ok(); ++i) {
		char uuid[16]; in.bytes(uuid, 16);
		std::string k(uuid, 16);
		switch (action) {
		case 0: {
			std::string name = in.str();
			int props = in.varint();
			for (int p = 0; p < props && in.ok(); ++p) { in.str(); in.str(); if (in.boolean()) in.str(); }
			in.varint(); in.varint();
			if (in.boolean()) in.str();
			playerNames[k] = name;
			break;
		}
		case 1: in.varint(); break;
		case 2: in.varint(); break;
		case 3: if (in.boolean()) in.str(); break;
		case 4: playerNames.erase(k); break;
		}
	}
}

// ---------------------------------------------------------------------------
// Local player actions
// ---------------------------------------------------------------------------
void EaglerClient::onStartDestroy(int x, int y, int z, int face)
{
	if (!isPlaying()) return;
	PacketOut o(0x07); o.u8(0); o.position(x + offX, y, z + offZ); o.u8(face); send(o);
	digging = true;
	onSwing();
}

void EaglerClient::onAbortDestroy()
{
	if (!isPlaying() || !digging) return;
	digging = false;
	PacketOut o(0x07); o.u8(1); o.position(0, 0, 0); o.u8(0); send(o);
}

void EaglerClient::onDestroyed(int x, int y, int z, int face)
{
	if (!isPlaying()) return;
	if (gameMode == 1) { // creative: starting to dig breaks instantly
		if (!digging) { PacketOut o(0x07); o.u8(0); o.position(x + offX, y, z + offZ); o.u8(face); send(o); }
	} else {
		if (!digging) { PacketOut s(0x07); s.u8(0); s.position(x + offX, y, z + offZ); s.u8(face); send(s); }
		PacketOut o(0x07); o.u8(2); o.position(x + offX, y, z + offZ); o.u8(face); send(o);
	}
	digging = false;
}

void EaglerClient::onUseItemOn(int x, int y, int z, int face, float cx, float cy, float cz)
{
	if (!isPlaying()) return;
	PacketOut o(0x08);
	o.position(x + offX, y, z + offZ);
	o.u8(face);
	writeSlot(o, mc->player->inventory->getSelected());
	o.u8((int)(Mth::clamp(cx, 0.0f, 1.0f) * 15)); o.u8((int)(Mth::clamp(cy, 0.0f, 1.0f) * 15)); o.u8((int)(Mth::clamp(cz, 0.0f, 1.0f) * 15));
	send(o);
	onSwing();
}

void EaglerClient::onUseItem()
{
	if (!isPlaying()) return;
	PacketOut o(0x08);
	o.i64(-1);
	o.u8(255);
	writeSlot(o, mc->player->inventory->getSelected());
	o.u8(0); o.u8(0); o.u8(0);
	send(o);
}

void EaglerClient::onAttack(Entity* target)
{
	if (!isPlaying() || !target) return;
	onSwing();
	PacketOut o(0x02); o.varint(target->entityId); o.varint(1); send(o);
}

void EaglerClient::onInteract(Entity* target)
{
	if (!isPlaying() || !target) return;
	PacketOut o(0x02); o.varint(target->entityId); o.varint(0); send(o);
}

void EaglerClient::onSwing()
{
	if (!isPlaying()) return;
	PacketOut o(0x0A);
	send(o);
}

int EaglerClient::biomeAt(int x, int z, void* ctx)
{
	EaglerClient* self = (EaglerClient*)ctx;
	const int sx = x + self->offX, sz = z + self->offZ;
	std::map<long long, CachedChunk*>::const_iterator it = self->cache.find(key(sx >> 4, sz >> 4));
	if (it == self->cache.end()) return -1;
	return it->second->biomes[((sz & 15) << 4) | (sx & 15)];
}

bool EaglerClient::isTerrainReady() const
{
	if (!isPlaying() || !positionKnown || !mc->player) return false;
	const int cx = Mth::floor(mc->player->x) >> 4, cz = Mth::floor(mc->player->z) >> 4;
	for (int dx = -1; dx <= 1; ++dx)
	for (int dz = -1; dz <= 1; ++dz)
		if (!localChunkReady(cx + dx, cz + dz)) return false;
	return true;
}

Entity* EaglerClient::live(int id)
{
	if (!level) return 0;
	Entity* e = level->getEntity(id);
	return (e && !e->removed) ? e : 0;
}

bool EaglerClient::swapIntoHotbar(Inventory* inv, int hotbarSlot, int inventorySlot)
{
	EaglerClient* self = s_active;
	if (!self || !self->mc->player || inv != self->mc->player->inventory) return false;
	if (hotbarSlot < 0 || hotbarSlot > 8 || inventorySlot < 9 || inventorySlot > 44) return true;
	const int hot = 36 + hotbarSlot;
	if (inventorySlot == hot) return true;
	// Java "number key" swap of the storage slot with the hotbar slot
	ItemInstance* a = inv->getItem(inventorySlot);
	ItemInstance* b = inv->getItem(hot);
	ItemInstance ia = a ? *a : ItemInstance(), ib = b ? *b : ItemInstance();
	self->sendWindowClick(0, inventorySlot, hotbarSlot, 2, ia);
	if (ib.isNull() || ib.count <= 0) inv->clearSlot(inventorySlot); else inv->replaceSlot(inventorySlot, &ib);
	if (ia.isNull() || ia.count <= 0) inv->clearSlot(hot); else inv->replaceSlot(hot, &ia);
	JavaInventoryScreen::fixHotbarLinks(inv);
	return true;
}
