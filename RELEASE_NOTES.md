## MCPE Extended 0.9.0

Minecraft Pocket Edition 0.6.1 source, updated toward PE 0.9.0, with Java Edition features and fixes.

### Downloads
- `mcpeextended-0.9.0-arm64-v8a.apk`: most phones from the last ~8 years
- `mcpeextended-0.9.0-armeabi-v7a.apk`: older 32-bit phones
- `mcpeextended-0.9.0-universal.apk`: both, if you're not sure

Android 5.0+ (API 21). If you have an older build installed and the install fails, uninstall it first; builds can be signed with different keys. Your worlds stay in `games/com.mojang`.

### New
- **Java Edition world border**: animated force field (blue, green while growing, red while shrinking), collision, damage beyond the safe zone, red warning vignette, no block interaction outside. `/worldborder set|add|center|get|damage|warning|reset`, using Java syntax. Saved per world.
- **Java view bobbing**: the hand now lags behind camera rotation (arm sway), and walk bobbing uses the distance actually moved.
- **Bigger worlds**: 512×512 blocks (4× the area). Existing 256×256 worlds load fine and get new terrain generated around them.
- **0.9.0 blocks** (Pocket Edition IDs): podzol, mycelium, red sand, packed ice, hardened clay, 16 stained clays, blue orchid, allium, azure bluet, 4 tulips, oxeye daisy, acacia and dark oak logs, leaves and planks, block of coal. Includes crafting and smelting recipes.
- **0.9.0 world generation**: acacia trees in savannas, dark oak in forests, new flowers by biome, podzol in taigas, red sand and terracotta badlands in deserts, ice spikes, mushroom-field patches.
- `/tp x y z` (supports `~`)

### Fixes: floating point precision
- Game clock is now monotonic and double precision. Frame timing no longer gets steppy after hours of uptime, or jumps when the system clock changes.
- Walk and limb animation counters wrap by whole periods, so bobbing and limb swing stay smooth in long sessions.
- Cloud drift computed in double precision.
- Android requests a 24-bit depth buffer (16-bit caused distant z-fighting/flicker).
- NBT doubles were being truncated to float.

### Fixes: stability and performance
- Chunk storage is zlib compressed (about 15× smaller saves). Old saves still load.
- Fixed an out-of-bounds read on every launch, out-of-bounds pointer math in models, and undefined integer overflow in world-gen hashes.
- Mob/player rotations were sent wrong in multiplayer on ARM devices (float→byte conversion).
- Typing quickly no longer cuts off the end of chat messages and commands.
- No audio device no longer spams errors every frame.
- The GL thread no longer spins at full speed while the app is in the background.
- Multiplayer protocol bumped to 10. Old 0.6.1 clients get a clear "outdated" message.
