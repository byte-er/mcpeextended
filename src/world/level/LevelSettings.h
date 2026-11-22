#ifndef NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__
#define NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__

//package net.minecraft.world.level;

namespace GameType {
	const int Undefined = -1;
	const int Survival = 0;
	const int Creative = 1;

	const int Default = Creative;
}

class LevelSettings
{
public:
    // World types offered on the 0.9.0 style Create World screen
    enum WorldType { WORLD_LARGE = 0, WORLD_OLD = 1, WORLD_FLAT = 2, WORLD_EMPTY = 3 /* Eaglercraft: filled from the server */,
                     WORLD_INFINITE = 4 /* 60,000,000 blocks: a 512x512 window that follows the player (see InfiniteWorld.h) */ };

    LevelSettings(long seed, int gameType, int worldType = WORLD_LARGE)
    :   seed(seed),
        gameType(gameType),
        worldType(worldType)
    {
    }
    int getWorldType() const { return worldType; }
	static LevelSettings None() {
		return LevelSettings(-1,-1);
	}

    long getSeed() const {
        return seed;
    }

    int getGameType() const {
        return gameType;
    }

	//
	// Those two should actually not be here
	// @todo: Move out when we add LevelSettings.cpp :p
	//
	static int validateGameType(int gameType) {
        switch (gameType) {
		case GameType::Creative:
		case GameType::Survival:
            return gameType;
        }
        return GameType::Default;
    }

	static std::string gameTypeToString(int gameType) {
		if (gameType == GameType::Survival) return "Survival";
		if (gameType == GameType::Creative) return "Creative";
		return "Undefined";
	}

private:
    const long seed;
    const int gameType;
    const int worldType;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__*/
