#ifndef NET_MINECRAFT_WORLD_ENTITY_NPC__NpcChat_H__
#define NET_MINECRAFT_WORLD_ENTITY_NPC__NpcChat_H__

// What the Player NPCs say (shared by the NPC source files).
namespace NpcChat {

enum Topic { T_WOOD, T_STONE, T_COAL, T_IRON, T_BUILD, T_BUILT, T_FARM, T_PEN, T_NIGHT, T_MORNING,
             T_HELLO, T_HELP, T_BANTER, T_HURT, T_DEATH, T_CRAFT, T_TOOLS, T_HARVEST,
             T_CAVE, T_DIAMOND, T_GRIEF, T_GRIEFED, T_ACCUSE, T_SORRY, T_TOWN, T_PROJECT, T_STORE,
             T_NEED, T_SMELT, T_GOLD, T_COUNT };

// how much a line matters when lots of NPCs are talking (0 small talk .. 2 always)
inline int importance(int topic)
{
	switch (topic) {
	case T_DEATH: case T_DIAMOND: case T_GRIEFED: case T_ACCUSE: case T_TOWN: return 2;
	case T_BANTER: case T_WOOD: case T_STONE: case T_CRAFT: case T_HELLO: case T_STORE: case T_HARVEST: return 0;
	default: return 1;
	}
}

static const char* const SAY_LINES[T_COUNT][6] = {
	{ "getting some wood", "need more logs", "chopping trees brb", "anyone seen birch around here", "wood run", "gonna grab some logs" },
	{ "going mining", "digging down for stone", "need cobble", "strip mining time", "gonna make a little mine", "need stone" },
	{ "found coal", "coal!", "got some coal", "nice coal vein", "torches incoming", "ooh coal" },
	{ "IRON", "found iron!!", "got iron ore", "iron down here", "finally some iron", "iron :D" },
	{ "starting my house", "building a base", "gonna build here", "house time", "this spot looks good for a house", "building lol" },
	{ "house done", "finished my house", "my house is done", "base is done :)", "ok house finished", "done building" },
	{ "making a wheat farm", "starting a farm", "farm time", "planting wheat", "gonna farm a bit", "need food so farm" },
	{ "making an animal pen", "building a pen", "gonna get some cows in a pen", "animal farm time", "fencing in some animals", "pen time" },
	{ "its getting dark", "night already?", "going inside", "mobs are out", "gonna stay in for the night", "night lol" },
	{ "morning", "its day again", "ok back to work", "sun is up", "good morning", "day time" },
	{ "hi", "hey", "hello", "yo", "hii", "sup" },
	{ "anyone need help", "ill help build", "need a hand?", "helping out", "lemme help", "want help with that" },
	{ "this world is nice", "lol", "what are you guys doing", "brb", "anyone got food", "wish we had more diamonds" },
	{ "ouch", "ow", "almost died lol", "help", "mobs everywhere", "that hurt" },
	{ "rip", "noo", "f", "rip lol", "oof", "rip bro" },
	{ "crafting", "making tools", "one sec crafting", "crafting stuff", "making sticks", "brb crafting" },
	{ "got a stone pickaxe", "stone tools now", "upgraded my tools", "new pickaxe", "finally better tools", "got my tools" },
	{ "harvesting wheat", "wheat is ready", "making bread", "farm is growing nicely", "got wheat", "replanting" },
	{ "found a cave", "going caving", "cave time", "theres a cave here", "exploring this cave", "caving brb" },
	{ "DIAMONDS", "found diamonds!!!", "DIAMONDS!!", "no way diamonds", "got diamonds :D", "diamonds lets go" },
	{ "lol", "get griefed", "oops", "wasnt me", "hehe", "nice house you had" },
	{ "who griefed my house", "WHO DID THIS", "someone broke my house", "bruh my house", "who touched my stuff", "really? my house??" },
	{ "stop griefing", "stop it", "why would you grief", "dont grief", "griefer!!", "not cool" },
	{ "ok ok sorry", "fine ill stop", "it was a joke", "sorry", "chill", "my bad" },
	{ "lets make a town", "we should build a town", "town time", "ok lets all build here", "this is our town now", "lets start a town" },
	{ "working on the roads", "building the town hall", "town project time", "helping with the town", "lets finish this", "big build time" },
	{ "put stuff in the chest", "dropping stuff in storage", "storage run", "filling the chests", "put my cobble in the chest", "deposited" },
	{ "anyone got cobble", "need more blocks", "we need wood", "out of blocks", "need materials", "chest is empty" },
	{ "smelting iron", "furnace time", "cooking iron", "smelting", "making iron", "need a furnace" },
	{ "found gold", "gold!", "shiny", "got gold ore", "gold lol", "ooh gold" },
};

static const char* const REPLY_LINES[T_COUNT][4] = {
	{ "ok", "can you get me some too", "theres trees over by my house", "nice" },
	{ "careful down there", "same", "get me some cobble", "ok" },
	{ "nice", "share some?", "lucky", "i need coal" },
	{ "no way", "nice!!", "where", "lucky" },
	{ "nice spot", "cool", "ill build next to you", "ok" },
	{ "looks good", "nice house", "cool", "nice" },
	{ "nice", "ill help with the farm", "we need food", "good idea" },
	{ "lol", "nice", "get some sheep too", "cool" },
	{ "same", "yeah", "zombies lol", "ok gn" },
	{ "morning", "gm", "finally", "yay" },
	{ "hi", "hey", "hello", "welcome" },
	{ "sure", "thanks", "ok", "yes please" },
	{ "lol", "idk", "same", "fr" },
	{ "you ok?", "lol", "run", "careful" },
	{ "rip", "f", "noo", "rip" },
	{ "ok", "", "nice", "" },
	{ "nice", "noice", "", "cool" },
	{ "nice", "can i have bread", "", "yum" },
	{ "careful", "bring torches", "can i come", "nice" },
	{ "WHERE", "no way", "share??", "lucky!!" },
	{ "bruh", "not funny", "stop", "why" },
	{ "wasnt me", "who would do that", "that sucks", "ill help fix it" },
	{ "yeah stop", "griefer lol", "ban him", "seriously" },
	{ "you better", "ok", "sure", "lol" },
	{ "yes", "ok!", "lets go", "im in" },
	{ "ill help", "nice", "on it", "ok" },
	{ "thanks", "nice", "", "ok" },
	{ "ill get some", "theres some in the chest", "mining now", "ok" },
	{ "nice", "", "can i have some", "ok" },
	{ "nice", "lucky", "", "gold lol" },
};

} // namespace NpcChat

#endif /*NET_MINECRAFT_WORLD_ENTITY_NPC__NpcChat_H__*/
