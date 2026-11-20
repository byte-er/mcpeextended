#ifndef NET_MINECRAFT_NETWORK_EAGLER__EaglerClient_H__
#define NET_MINECRAFT_NETWORK_EAGLER__EaglerClient_H__

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <set>

class Minecraft;
class MultiPlayerLevel;
class Entity;
class PacketIn;
class PacketOut;
class ItemInstance;

/**
 * Client for Eaglercraft 1.8.9 servers (EaglerXServer / EaglerXBungee).
 *
 * Transport: one WebSocket (AppPlatform::ws*), binary frames. First the
 * Eaglercraft handshake (protocol v3), then plain Minecraft 1.8 packets, one
 * per frame, uncompressed.
 *
 * World: the server's world is infinite and 256 high; this engine keeps a
 * finite 512x512x128 level. Received chunks are cached in server coordinates
 * and copied into the level through a sliding window: local = server - offset.
 * When the player nears the window edge the window is re-centred and rebuilt
 * from the cache. Blocks above y=127 are dropped. Overworld only.
 */
class EaglerClient
{
public:
	enum State { CONNECTING, HANDSHAKE, LOGIN, PLAY, FAILED, CLOSED };

	EaglerClient(Minecraft* mc, const std::string& address, const std::string& username);
	~EaglerClient();

	/** Called every game tick (also before a level exists). */
	void tick();

	State getState() const { return state; }
	const std::string& getStatus() const { return status; }
	const std::string& getError() const { return error; }
	bool isPlaying() const { return state == PLAY && level != 0; }
	/** Placed by the server and the chunks around the player have arrived. */
	bool isTerrainReady() const;
	static int biomeAt(int x, int z, void* ctx);
	static bool swapIntoHotbar(class Inventory* inv, int hotbarSlot, int inventorySlot);
	static EaglerClient* s_active;
	/** Normalises user input like "eagarchy.wackify.dev" to "wss://eagarchy.wackify.dev/". */
	static std::string normaliseAddress(const std::string& in);
	static std::string sanitiseUsername(const std::string& in);

	// ---- hooks from the game (local player actions) ----
	void onStartDestroy(int x, int y, int z, int face);
	void onAbortDestroy();
	void onDestroyed(int x, int y, int z, int face);
	void onUseItemOn(int x, int y, int z, int face, float cx, float cy, float cz);
	void onUseItem();
	void onAttack(Entity* target);
	void onInteract(Entity* target);
	void onSwing();
	void sendChat(const std::string& message);
	void sendWindowClick(int window, int slot, int button, int mode, const ItemInstance& clicked);
	void sendCloseWindow(int window);
	void requestRespawn();

private:
	struct CachedChunk {
		unsigned char blocks[16 * 16 * 128];
		unsigned char meta[16 * 16 * 64];
		unsigned char sky[16 * 16 * 64];
		unsigned char light[16 * 16 * 64];
		unsigned char biomes[256];
	};
	struct TrackedEntity {
		int id;                  // entity id (looked up each time: the game may delete entities)
		int fx, fy, fz;          // server position, 1/32 block fixed point
		float yaw, pitch;
		bool isPlayer;
		int pendingItemType;     // object type 2 (item) waiting for its metadata
		TrackedEntity() : id(0), fx(0), fy(0), fz(0), yaw(0), pitch(0), isPlayer(false), pendingItemType(0) {}
	};

	void send(const PacketOut& p);
	void sendHandshakeVersion();
	void handleHandshake(const std::vector<unsigned char>& msg);
	void handlePlay(const std::vector<unsigned char>& msg);
	void fail(const std::string& why);

	// play packet handlers
	void onJoinGame(PacketIn& in);
	void onChat(PacketIn& in);
	void onPositionLook(PacketIn& in);
	void onChunk(PacketIn& in);
	void onChunkBulk(PacketIn& in);
	void readChunkColumn(PacketIn& in, int cx, int cz, int mask, bool groundUp, bool skylight);
	void onBlockChange(int x, int y, int z, int state);
	void onMultiBlockChange(PacketIn& in);
	void onSpawnPlayer(PacketIn& in);
	void onSpawnMob(PacketIn& in);
	void onSpawnObject(PacketIn& in);
	void onMetadata(PacketIn& in);
	void onDestroyEntities(PacketIn& in);
	void onEntityMove(PacketIn& in, bool move, bool look);
	void onEntityTeleport(PacketIn& in);
	void onVelocity(PacketIn& in);
	void onPlayerList(PacketIn& in);
	void onWindowItems(PacketIn& in);
	void onSetSlot(PacketIn& in);
	void onWorldBorder(PacketIn& in);
	void onExplosion(PacketIn& in);
	void onEquipment(PacketIn& in);
	void onPluginMessage(PacketIn& in);
	void onOpenWindow(PacketIn& in);
	void applyWindowSlot(int window, int slot, const ItemInstance& it, bool has);

	void createLevel();
	void applyCachedWindow();
	void applyChunkToLevel(int scx, int scz, const CachedChunk* c);
	void recentreIfNeeded();
	void placeEntity(TrackedEntity& t, bool snap);
	void sendMovement();
	void syncHeldItem();
	bool readSlot(PacketIn& in, ItemInstance& out);
	void writeSlot(PacketOut& out, const ItemInstance* item);
	void skipMetadata(PacketIn& in);
	bool chunkInWindow(int scx, int scz) const;
	bool localChunkReady(int lcx, int lcz) const;
	void removeTracked(int id);
	Entity* live(int id);
	static std::string chatToText(const std::string& json);
	static long long key(int x, int z) { return ((long long)x << 32) ^ (unsigned int)z; }

	Minecraft* mc;
	std::string address, username;
	State state;
	std::string status, error;
	int ticks;

	MultiPlayerLevel* level;
	bool levelReady;
	std::deque<std::vector<unsigned char> > pendingPlay; // packets received before the level was generated

	// world window
	bool haveOffset;
	int offX, offZ;          // block offset, multiple of 16
	std::map<long long, CachedChunk*> cache;

	// local player
	int playerEid;
	int gameMode;
	bool positionKnown;
	bool released;           // placed after the last server teleport
	bool held;               // kept in place this tick (terrain missing)
	double holdX, holdY, holdZ; // server-requested position while waiting for chunks
	double lastX, lastY, lastZ;
	float lastYaw, lastPitch;
	int ticksSinceMove;
	int lastSelected;
	int lastHeldId, lastHeldData;
	bool digging;
	int openWindowId;
	int actionCounter;

	// world border (server coordinates)
	double borderX, borderZ;

	std::map<int, TrackedEntity> entities;
	std::map<std::string, std::string> playerNames; // uuid bytes -> name
	std::set<std::string> requestedSkins;
};

#endif /*NET_MINECRAFT_NETWORK_EAGLER__EaglerClient_H__*/
