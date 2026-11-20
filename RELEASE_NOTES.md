## MCPE Extended 0.9.3

### Downloads
| Platform | File |
|---|---|
| Android (most phones) | `mcpeextended-0.9.3-arm64-v8a.apk` (or `-universal.apk` if unsure, `-armeabi-v7a.apk` for old 32-bit phones) |
| Any computer, no setup | `mcpeextended.html`: one self-contained file (about 15 MB). Download it and open it in Chrome, Edge, Firefox or Safari; it works offline and saves worlds in the browser. |
| PC (Windows / macOS / Linux), **iOS**, other devices | `mcpeextended-web.zip`: the browser build. Host the files on any web server (HTTPS to install it as an app). On PC it uses mouse + keyboard (click to capture the mouse, Esc pauses); on phones and tablets it uses touch controls. On iPhone/iPad open it in Safari → Share → **Add to Home Screen** to install it as a fullscreen app. |

If installing the APK over an older build fails, uninstall the old one first; builds can be signed with different keys. Old worlds in `/sdcard/games/com.mojang` are copied over automatically when Android lets the app read them.

### New: join Eaglercraft 1.8.9 servers
- **Join Game → Eaglercraft Server**: enter the address (e.g. `wss://eagarchy.wackify.dev`). Works in the Android app and the browser build, on phones and PCs.
- Core gameplay: the server's world and lighting, walking and flying, breaking and placing blocks, other players and mobs, dropped items, chat and server `/commands` (e.g. `/register` and `/login` on servers with a login plugin), health, death and respawn, survival inventory, creative mode, the world border, and grass/leaf colours from the server's biomes.
- Limits: overworld only (no Nether or End). Blocks above height 127 aren't shown. Blocks this game doesn't have are replaced with similar ones. Crafting, chests and furnaces on servers aren't supported yet.
- Server kick messages (rate limits, login timeouts) are shown in full.

### New: skins
- **Options → Skin**: Steve, Alex, or **Custom**. Choosing Custom opens a file picker for a 64×64 (or old 64×32) skin PNG. It works in the browser and on Android. You'll see the skin in third person and on your first-person hand.
- On Eaglercraft servers your skin is sent to other players, and you see theirs (custom and preset skins).

### Fixes (PC)
- **Crafting**: the PC inventory (keyboard and mouse) had no way to open crafting. It now has a **Craft** button next to Armor. The Armor and Craft buttons also no longer disappear when your hand is empty. Press **C** to open crafting directly; **Esc** or **E** closes the inventory.
- **GUI scale**: the pause menu now has **Options**, so GUI scale (and everything else) can be changed in game, and closing Options returns you to the game. Screens such as Create World no longer stretch or split at small GUI scales.
- Icons for acacia and dark oak planks were missing in the crafting list. Missing block descriptions are filled in.
- `/give <id> [count] [data]` in single player.

---

## MCPE Extended 0.9.2

### Downloads
| Platform | File |
|---|---|
| Android (most phones) | `mcpeextended-0.9.2-arm64-v8a.apk` (or `-universal.apk` if unsure, `-armeabi-v7a.apk` for old 32-bit phones) |
| Any computer, no setup | `mcpeextended.html`: one self-contained file (about 15 MB). Download it and open it in Chrome, Edge, Firefox or Safari; it works offline and saves worlds in the browser. |
| PC (Windows / macOS / Linux), **iOS**, other devices | `mcpeextended-web.zip`: the browser build. Host the files on any web server (HTTPS to install it as an app). On PC it uses mouse + keyboard (click to capture the mouse, Esc pauses); on phones and tablets it uses touch controls. On iPhone/iPad open it in Safari → Share → **Add to Home Screen** to install it as a fullscreen app. |

If installing the APK over an older build fails, uninstall the old one first; builds can be signed with different keys. Old worlds in `/sdcard/games/com.mojang` are copied over automatically when Android lets the app read them.

### Fixes
- **See-through holes in the world**: distant terrain no longer skips areas whose chunks are still being rebuilt.
- **"Back to game" soft lock** in the browser build on PC.
- **Distant Terrain has quality levels**: full textured blocks right past your render distance, then 2×2 and 4×4 detail further out, instead of one very low level everywhere.

### New: closer to the real 0.9.0
- **Create a World screen** from 0.9.0, with world types **Old** (256×256 with a border), **Large** and **Flat**, and Survival / Creative.
- **Biome colours**: grass, tall grass, ferns and leaves are tinted per biome with the Java colormaps, blended smoothly across biome borders. Lush green forests, yellow-olive savannas and deserts, blue-green snowy plains, murky swamps. Grass block sides use the tinted overlay, so the sides match the top.
- **3D clouds**: thick 12×4×12-block volumetric clouds like 0.9.0. Turn off Fancy Graphics for the old flat clouds.
- **Touch HUD**: pause and chat buttons in the top right, using the 0.9.0 icons. Chat opens the keyboard with the text box at the top of the screen, and works for `/commands`.
- Fancy graphics and smooth lighting are on by default, as in 0.9.0.
- Browser build on phones and tablets: tapping a text field or the chat button opens the on-screen keyboard. On iPhone/iPad, tap the "Tap to type" bar.

### Notes
- The browser build is single-player; LAN multiplayer isn't available in browsers.
- iOS is supported through the browser/PWA build. A native iOS app would need an Apple developer account to sign.

---

## MCPE Extended 0.9.1

### Downloads
| Platform | File |
|---|---|
| Android (most phones) | `mcpeextended-0.9.1-arm64-v8a.apk` (or `-universal.apk` if unsure, `-armeabi-v7a.apk` for old 32-bit phones) |
| Any computer, no setup | `mcpeextended.html`: one self-contained file (about 15 MB). Download it and open it in Chrome, Edge, Firefox or Safari; it works offline and saves worlds in the browser. |
| PC (Windows / macOS / Linux), **iOS**, other devices | `mcpeextended-web.zip`: the browser build. Host the files on any web server (HTTPS to install it as an app). On PC it uses mouse + keyboard (click to capture the mouse, Esc pauses); on phones and tablets it uses touch controls. On iPhone/iPad open it in Safari → Share → **Add to Home Screen** to install it as a fullscreen app. |

If installing the APK over an older build fails, uninstall the old one first; builds can be signed with different keys. Old worlds in `/sdcard/games/com.mojang` are copied over automatically when Android lets the app read them.

### Fixes
- **World saves on Android**: worlds are now stored in the app's own folder, which needs no permission. Previously they silently failed to save on Android 6+.
- The game saves and opens the pause menu when you leave the app.
- **"Deep-fried" colours on Android**: the GL surface could get an alpha channel, which Android blended into the screen. Fixed.
- **3D anaglyph mode** now renders properly (Java's colour conversion was missing).
- The touch circle and crack overlay no longer stay on screen after breaking a block or lifting your finger.

### New
- **Caves and ravines** (Java Beta cave generator, previously disabled).
- **Distant Terrain (LOD)**: low-detail terrain beyond your render distance, so you can see the whole world to the horizon. Toggle it under Options → Graphics.
- **0.9.0 menus**: rotating panorama title screen, 0.9.0 GUI textures, "Play" button, tidier Create World screen.
- **Browser build (PC, iOS and more)**: installs as a PWA. It supports mouse + keyboard on PC and touch controls on phones and tablets, screen rotation, offline play, and saves stored in the browser.

### Notes
- The browser build is single-player; LAN multiplayer isn't available in browsers.
- iOS is supported through the browser/PWA build. A native iOS app would need an Apple developer account to sign.

---

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
