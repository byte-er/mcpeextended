#include "ConsoleScreen.h"
#include "../../../network/eagler/EaglerClient.h"
#include "../Gui.h"
#include "../../Minecraft.h"
#include "../../player/LocalPlayer.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../world/level/Level.h"
#include "../../../world/level/WorldBorder.h"
#include "../../../world/level/storage/LevelData.h"
#include "../../particle/ParticleEngine.h"
#include <cmath>
#include "../../../world/entity/MobFactory.h"
#include "../../../network/RakNetInstance.h"
#include "../../../network/ServerSideNetworkHandler.h"
#include "../../../network/packet/ChatPacket.h"
#include "../../../platform/log.h"
#include "../../../util/Mth.h"
#include "../../../world/item/Item.h"
#include "../../../world/item/ItemInstance.h"
#include "../../../world/entity/player/Inventory.h"

#include <sstream>
#include <cstdlib>
#include <cctype>

ConsoleScreen::ConsoleScreen(const std::string& initial)
:   _input(initial),
    _cursorBlink(0)
{
}

void ConsoleScreen::init()
{
    if (minecraft->useTouchscreen())
        minecraft->platform()->showKeyboard();
}

void ConsoleScreen::removed()
{
    if (minecraft->useTouchscreen())
        minecraft->platform()->hideKeyboard();
}

void ConsoleScreen::tick()
{
    _cursorBlink++;
}

bool ConsoleScreen::handleBackEvent(bool /*isDown*/)
{
    minecraft->setScreen(NULL);
    return true;
}

void ConsoleScreen::keyPressed(int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE) {
        minecraft->setScreen(NULL);
    } else if (eventKey == Keyboard::KEY_RETURN) {
        execute();
    } else if (eventKey == Keyboard::KEY_BACKSPACE) {
        if (!_input.empty())
            _input.erase(_input.size() - 1, 1);
    } else {
        super::keyPressed(eventKey);
    }
}

void ConsoleScreen::keyboardNewChar(char inputChar)
{
    if (inputChar >= 32 && inputChar < 127)
        _input += inputChar;
}

// ---------------------------------------------------------------------------
// execute: run _input as a command, print result, close screen
// ---------------------------------------------------------------------------
void ConsoleScreen::execute()
{
    if (_input.empty()) {
        minecraft->setScreen(NULL);
        return;
    }
    if (minecraft->eagler && minecraft->eagler->isPlaying()) {
        // on an Eaglercraft server: chat and /commands are the server's
        minecraft->eagler->sendChat(_input);
        minecraft->setScreen(NULL);
        return;
    }

    if (_input[0] == '/') {
        // Command
        std::string result = processCommand(_input);
        if (!result.empty())
            minecraft->gui.addMessage(result);
    } else {
        // Chat message: <name> message
        std::string msg = std::string("<") + minecraft->player->name + "> " + _input;
        if (minecraft->netCallback && minecraft->raknetInstance->isServer()) {
            // Hosting a LAN game: displayGameMessage shows locally + broadcasts MessagePacket to clients
            static_cast<ServerSideNetworkHandler*>(minecraft->netCallback)->displayGameMessage(msg);
        } else if (minecraft->netCallback) {
            // Connected client: send ChatPacket to server; server echoes it back as MessagePacket
            ChatPacket chatPkt(msg);
            minecraft->raknetInstance->send(chatPkt);
        } else {
            // Singleplayer: show locally only
            minecraft->gui.addMessage(msg);
        }
    }

    minecraft->setScreen(NULL);
}

// ---------------------------------------------------------------------------
// processCommand
// ---------------------------------------------------------------------------
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

std::string ConsoleScreen::processCommand(const std::string& raw)
{
    // strip leading '/'
    std::string line = raw;
    if (!line.empty() && line[0] == '/') line = line.substr(1);
    line = trim(line);

    // tokenise
    std::vector<std::string> args;
    {
        std::istringstream ss(line);
        std::string tok;
        while (ss >> tok) args.push_back(tok);
    }

    if (args.empty()) return "";

    Level* level = minecraft->level;
    if (!level) return "No level loaded.";

    // -----------------------------------------------------------------------
    // /time ...
    // -----------------------------------------------------------------------
    if (args[0] == "time") {
        if (args.size() < 2)
            return "Usage: /time <add|set|query> ...";

        const std::string& sub = args[1];

        // -- time add <value> -----------------------------------------------
        if (sub == "add") {
            if (args.size() < 3) return "Usage: /time add <value>";
            long delta = std::atol(args[2].c_str());
            long newTime = level->getTime() + delta;
            level->setTime(newTime);
            std::ostringstream out;
            out << "Set the time to " << (newTime % Level::TICKS_PER_DAY);
            return out.str();
        }

        // -- time set <value|day|night|noon|midnight> -----------------------
        if (sub == "set") {
            if (args.size() < 3) return "Usage: /time set <value|day|night|noon|midnight>";
            const std::string& val = args[2];

            long t = -1;
            if      (val == "day")      t = 1000;
            else if (val == "noon")     t = 6000;
            else if (val == "night")    t = 13000;
            else if (val == "midnight") t = 18000;
            else {
                // numeric — accept positive integers only
                bool numeric = true;
                for (char c : val)
                    if (!std::isdigit((unsigned char)c)) { numeric = false; break; }
                if (!numeric) return std::string("Unknown value: ") + val;
                t = std::atol(val.c_str());
            }

            // Preserve the total day count so only the time-of-day changes
            long dayCount = level->getTime() / Level::TICKS_PER_DAY;
            long newTime  = dayCount * Level::TICKS_PER_DAY + (t % Level::TICKS_PER_DAY);
            level->setTime(newTime);
            std::ostringstream out;
            out << "Set the time to " << t;
            return out.str();
        }

        // -- time query <daytime|gametime|day> ------------------------------
        if (sub == "query") {
            if (args.size() < 3) return "Usage: /time query <daytime|gametime|day>";
            const std::string& what = args[2];

            long total   = level->getTime();
            long daytime = total % Level::TICKS_PER_DAY;
            long day     = total / Level::TICKS_PER_DAY;

            std::ostringstream out;
            if      (what == "daytime")  { out << "The time of day is " << daytime; }
            else if (what == "gametime") { out << "The game time is "   << total;   }
            else if (what == "day")      { out << "The day is "         << day;     }
            else return std::string("Unknown query: ") + what;
            return out.str();
        }

        return "Unknown sub-command. Usage: /time <add|set|query> ...";
    }

    // -----------------------------------------------------------------------
    // /tp <x> <y> <z>   (~ = current coordinate)
    // -----------------------------------------------------------------------
    if (args[0] == "give") {
        // /give <id> [count] [data]   (gives to yourself)
        if (args.size() < 2) return "Usage: /give <id> [count] [data]";
        int id = std::atoi(args[1].c_str());
        int count = args.size() > 2 ? std::atoi(args[2].c_str()) : 1;
        int data = args.size() > 3 ? std::atoi(args[3].c_str()) : 0;
        if (id <= 0 || id >= 512 || !Item::items[id]) return "Unknown item id " + args[1];
        if (count < 1) count = 1;
        if (count > 64 * 36) count = 64 * 36;
        int given = 0;
        while (given < count) {
            int n = Mth::Min(count - given, Item::items[id]->getMaxStackSize());
            ItemInstance stack(id, n, data);
            if (!minecraft->player->inventory->add(&stack)) break;
            given += n;
        }
        std::ostringstream out;
        out << "Gave " << given << " of item " << id;
        return out.str();
    }

    // /summon <mob>   (Java names: zombie, creeper, enderman, pig, ...)
    if (args[0] == "summon") {
        if (level->isClientSide) return "Only the host can summon.";
        if (args.size() < 2) return "Usage: /summon <mob>";
        std::string n = args[1];
        if (n.compare(0, 10, "minecraft:") == 0) n = n.substr(10);
        struct M { const char* name; int type; };
        static const M mobs[] = { { "chicken", MobTypes::Chicken }, { "cow", MobTypes::Cow }, { "pig", MobTypes::Pig },
            { "sheep", MobTypes::Sheep }, { "zombie", MobTypes::Zombie }, { "creeper", MobTypes::Creeper },
            { "skeleton", MobTypes::Skeleton }, { "spider", MobTypes::Spider }, { "zombie_pigman", MobTypes::PigZombie },
            { "pigzombie", MobTypes::PigZombie }, { "enderman", MobTypes::Enderman } };
        int type = 0;
        for (unsigned int i = 0; i < sizeof(mobs) / sizeof(mobs[0]); ++i) if (n == mobs[i].name) type = mobs[i].type;
        if (!type) return "Unknown mob " + args[1];
        LocalPlayer* p = minecraft->player;
        Mob* mob = MobFactory::CreateMob(type, level);
        if (!mob) return "Can't summon " + args[1];
        const float yr = p->yRot * Mth::DEGRAD;
        mob->moveTo(p->x - Mth::sin(yr) * 3, p->y - p->heightOffset, p->z + Mth::cos(yr) * 3, p->yRot + 180, 0);
        level->addEntity(mob);
        return "Summoned " + n;
    }

    if (args[0] == "tp" || args[0] == "teleport") {
        if (level->isClientSide) return "Only the host can teleport.";
        if (args.size() < 4) return "Usage: /tp <x> <y> <z>";
        LocalPlayer* p = minecraft->player;
        if (level->isInfinite()) {
            // infinite worlds: world coordinates; the window moves to the target
            const LevelData* d = level->getLevelData();
            double w[3] = { p->x + d->originX * 16.0, p->y - p->heightOffset, p->z + d->originZ * 16.0 };
            for (int i = 0; i < 3; ++i) {
                const std::string& a = args[1 + i];
                if (!a.empty() && a[0] == '~') w[i] += (a.size() > 1) ? std::atof(a.c_str() + 1) : 0.0;
                else w[i] = std::atof(a.c_str());
            }
            const double lim = 29999984.0; // inside the 60,000,000 block world border
            w[0] = w[0] < -lim ? -lim : w[0] > lim ? lim : w[0];
            w[2] = w[2] < -lim ? -lim : w[2] > lim ? lim : w[2];
            if (w[1] < 1) w[1] = 1;
            const double lx = w[0] - d->originX * 16.0, lz = w[2] - d->originZ * 16.0;
            const double mid = LEVEL_WIDTH * 0.5;
            if (lx < mid - 96 || lx > mid + 96 || lz < mid - 96 || lz > mid + 96) {
                level->shiftWindow((int)std::floor((lx - mid) / 16.0), (int)std::floor((lz - mid) / 16.0));
                minecraft->particleEngine->clear();
            }
            const float x = (float)(w[0] - level->getLevelData()->originX * 16.0);
            const float z = (float)(w[2] - level->getLevelData()->originZ * 16.0);
            p->moveTo(x, (float)w[1] + p->heightOffset, z, p->yRot, p->xRot);
            p->xd = p->yd = p->zd = 0;
            p->fallDistance = 0;
            std::ostringstream out;
            out.precision(10);
            out << "Teleported to " << (long long)std::floor(w[0]) << ", " << (int)w[1] << ", " << (long long)std::floor(w[2]);
            return out.str();
        }
        float c[3] = { p->x, p->y - p->heightOffset, p->z };
        for (int i = 0; i < 3; ++i) {
            const std::string& a = args[1 + i];
            if (!a.empty() && a[0] == '~') c[i] += (a.size() > 1) ? (float)std::atof(a.c_str() + 1) : 0.0f;
            else c[i] = (float)std::atof(a.c_str());
        }
        // keep the player inside the finite world (outside is solid bedrock)
        const float pad = p->bbWidth * 0.5f + 0.01f;
        c[0] = Mth::clamp(c[0], pad, (float)LEVEL_WIDTH - pad);
        c[2] = Mth::clamp(c[2], pad, (float)LEVEL_DEPTH - pad);
        if (c[1] < 1) c[1] = 1;
        p->moveTo(c[0], c[1] + p->heightOffset, c[2], p->yRot, p->xRot);
        p->xd = p->yd = p->zd = 0;
        p->fallDistance = 0;
        std::ostringstream out;
        out << "Teleported to " << c[0] << ", " << c[1] << ", " << c[2];
        return out.str();
    }

    // -----------------------------------------------------------------------
    // /worldborder ...   (Java Edition syntax)
    // -----------------------------------------------------------------------
    if (args[0] == "worldborder") {
        if (level->isClientSide) return "Only the host can change the world border.";
        WorldBorder& border = level->getWorldBorder();
        const std::string usage = "Usage: /worldborder <set|add|center|get|damage|warning|reset> ...";
        if (args.size() < 2) return usage;
        const std::string& sub = args[1];
        std::ostringstream out;
        out.setf(std::ios::fixed); out.precision(1);

        if (sub == "get") {
            out << "The world border is currently " << border.getSize() << " blocks wide";
            return out.str();
        }
        if (sub == "set" || sub == "add") {
            if (args.size() < 3) return "Usage: /worldborder " + sub + " <size> [seconds]";
            double v = std::atof(args[2].c_str());
            double target = (sub == "add") ? border.getSize() + v : v;
            if (target < 1.0) target = 1.0;
            if (target > WorldBorder::MAX_SIZE) target = WorldBorder::MAX_SIZE;
            long long secs = args.size() >= 4 ? std::atol(args[3].c_str()) : 0;
            if (secs > 0) {
                border.lerpSizeBetween(border.getSize(), target, secs * 1000LL);
                out << (target < border.getSize() ? "Shrinking" : "Growing")
                    << " the world border to " << target << " blocks wide over " << secs << " seconds";
            } else {
                border.setSize(target);
                out << "Set the world border to " << target << " blocks wide";
            }
            return out.str();
        }
        if (sub == "center") {
            if (args.size() < 4) return "Usage: /worldborder center <x> <z>";
            LocalPlayer* p = minecraft->player;
            double cx = (args[2] == "~") ? (double)p->x : std::atof(args[2].c_str());
            double cz = (args[3] == "~") ? (double)p->z : std::atof(args[3].c_str());
            border.setCenter(cx, cz);
            out << "Set the center of the world border to " << cx << ", " << cz;
            return out.str();
        }
        if (sub == "damage") {
            if (args.size() < 4) return "Usage: /worldborder damage <amount|buffer> <value>";
            double v = std::atof(args[3].c_str());
            if (v < 0) v = 0;
            if (args[2] == "amount") { border.setDamagePerBlock(v); out << "Set the world border damage to " << v << " per block each second"; }
            else if (args[2] == "buffer") { border.setDamageSafeZone(v); out << "Set the world border damage buffer to " << v << " blocks"; }
            else return "Usage: /worldborder damage <amount|buffer> <value>";
            return out.str();
        }
        if (sub == "warning") {
            if (args.size() < 4) return "Usage: /worldborder warning <distance|time> <value>";
            int v = std::atoi(args[3].c_str());
            if (v < 0) v = 0;
            if (args[2] == "distance") { border.setWarningBlocks(v); out << "Set the world border warning distance to " << v << " blocks"; }
            else if (args[2] == "time") { border.setWarningTime(v); out << "Set the world border warning time to " << v << " seconds"; }
            else return "Usage: /worldborder warning <distance|time> <value>";
            return out.str();
        }
        if (sub == "reset") {
            border.resetToLevel(LEVEL_WIDTH, LEVEL_DEPTH);
            return "Reset the world border to the edge of the world";
        }
        return usage;
    }

    return std::string("Unknown command: /") + args[0];
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void ConsoleScreen::render(int /*xm*/, int /*ym*/, float /*a*/)
{
    // Dim the game world slightly
    fillGradient(0, 0, width, height, 0x00000000, 0x40000000);

    const int boxH  = 12;
    // on touch screens the soft keyboard covers the bottom half: type at the top
    const int boxY  = minecraft->useTouchscreen() ? 2 : height - boxH - 2;
    const int boxX0 = 2;
    const int boxX1 = width - 2;

    // Input box background
    fill(boxX0, boxY, boxX1, boxY + boxH, 0xc0000000);
    // Border
    fill(boxX0,     boxY,          boxX1, boxY + 1,   0xff808080);
    fill(boxX0,     boxY + boxH - 1, boxX1, boxY + boxH, 0xff808080);
    fill(boxX0,     boxY,          boxX0 + 1, boxY + boxH, 0xff808080);
    fill(boxX1 - 1, boxY,          boxX1,     boxY + boxH, 0xff808080);

    // Input text + blinking cursor
    std::string displayed = _input;
    if ((_cursorBlink / 10) % 2 == 0)
        displayed += '_';

    // Placeholder hint when empty
    if (_input.empty() && (_cursorBlink / 10) % 2 != 0)
        font->drawShadow("Type a message or /command", (float)(boxX0 + 2), (float)(boxY + 2), 0xff606060);
    else
        font->drawShadow(displayed, (float)(boxX0 + 2), (float)(boxY + 2), 0xffffffff);
}
