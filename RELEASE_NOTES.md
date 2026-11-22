## MCPE Extended 0.9.7

### Downloads
| Platform | File |
|---|---|
| Android (most phones) | `mcpeextended-0.9.7-arm64-v8a.apk` (or `-universal.apk` if unsure, `-armeabi-v7a.apk` for old 32-bit phones) |
| Any computer, no setup | `mcpeextended.html`: one self-contained file (about 15 MB). Download it and open it in Chrome, Edge, Firefox or Safari; it works offline and saves worlds in the browser. |
| PC (Windows / macOS / Linux), **iOS**, other devices | `mcpeextended-web.zip`: the browser build. Host the files on any web server (HTTPS to install it as an app). On PC it uses mouse + keyboard (click to capture the mouse, Esc pauses); on phones and tablets it uses touch controls. On iPhone/iPad open it in Safari → Share → **Add to Home Screen** to install it as a fullscreen app. |

If installing the APK over an older build fails, uninstall the old one first; builds can be signed with different keys. Old worlds in `/sdcard/games/com.mojang` are copied over automatically when Android lets the app read them.

### New: Infinite worlds
- **Create a World -> Infinite** replaces Large. Infinite worlds are **60,000,000 blocks** across, with Java's world border at 30 million blocks in each direction.
- **Bigger biomes**: four times the old size in each direction, closer to Java.
- **New save format**: infinite worlds store chunks in region files (`region/r.<x>.<z>.dat`, 32x32 chunks each), so a world's size on disk only depends on how much you've explored.
- Terrain stays correct millions of blocks out: the terrain noise now uses double precision.
- How it works: the game keeps the 512x512 blocks around you loaded and moves that area with you as you walk. Mobs, chests and furnaces you walk away from are saved and come back when you return.
- `/tp x y z` and the F3 screen use real world coordinates in infinite worlds (e.g. `/tp 1000000 100 -2500000`).
- Infinite worlds aren't shared on LAN.
- Old and Flat worlds, and existing Large worlds, work as before.

### New: Endermen
- They spawn rarely at night, like in Java. They're neutral until you **look them in the eyes** or hit them; then their jaw drops and they come for you.
- They **teleport** when hurt, when in water and sometimes in daylight. Arrows never hit them; they teleport away instead.
- They **pick up and put down blocks** (grass, dirt, sand, gravel, flowers, mushrooms, cacti, clay, melons, TNT, mycelium, podzol), and you can see the block in their hands.
- Glowing purple eyes, 40 health, 7 damage.
- There's an **Enderman spawn egg** in the creative tools tab, and Endermen show up on Eaglercraft servers.
- New command: **`/summon <mob>`** (zombie, creeper, skeleton, spider, enderman, pig, cow, sheep, chicken, zombie_pigman).

### Wood names and crafting
- Logs are now called **Oak wood, Spruce wood, Birch wood, Acacia wood** and **Dark oak wood**. Planks are **Oak planks, Spruce planks**, and so on.
- **Each log crafts its own planks**: spruce wood makes spruce planks, birch wood makes birch planks. Spruce and birch planks are new, with their own textures, and are in the creative inventory.

### Fixes
- **Menus cut off at the bottom**: the options lists measured their own height wrong, so you couldn't scroll to the last settings. They now scroll all the way.
- **Newer iPhones**: the browser build now stays clear of the Dynamic Island, the home indicator and the rounded corners, so no buttons are hidden behind them.
- **X-ray glitch in the middle of the world**: worlds made when levels were 256x256 kept that old world border. In today's bigger worlds it ran through the middle, at x/z 256, and its force field showed through the terrain there. Those worlds now get the right border, and the force field no longer draws through blocks.
- **Flat worlds are all plains** now, with no biome colour patches.

---

## Earlier: MCPE Extended 0.9.6

### New: the 0.9.0 mobile look
Rebuilt from screenshots of Pocket Edition 0.9.0, using the textures from the 0.9.0 APK.
- **Title screen**: the logo with a yellow **splash text** (all 296 splashes from 0.9.0, plus the Christmas, New Year and Halloween ones), one wide **Play** button, and the **tools** button in the bottom right corner for Options. The version and copyright are in the bottom left. **Join Game** moved to the world list (top bar), where 0.9.0 listed LAN games.
- **Creative inventory**: the 0.9.0 tabbed screen. It has an X button, four tabs (items, tools, decoration and blocks) and an 8-column grid that you scroll with your finger, with a scroll bar. Tapping an item puts it in your hotbar, and the screen stays open until you tap X. The decoration and tools tabs use 0.9.0's order. Wide phones get more columns instead of huge cells.
- **Spawn eggs** (chicken, cow, pig, sheep, creeper, skeleton, spider, zombie, zombie pigman) with 0.9.0's egg icons, in the creative tools tab. On Eaglercraft servers, spawn eggs show the right mob.
- The creative inventory now also has flint and steel, shears, clock, compass, an iron pickaxe, tall grass, fern, every dye, and all other items the game has.
- **Options**: a **Back** button in the title bar and square icon tabs (Game, Controls, Graphics, Sound, Account). Graphics starts with 0.9.0's **View Distance**, **Fancy Graphics**, **Beautiful skies** (sky and clouds) and **Animated water**, and these are now saved. Long lists scroll with your finger. Username, Skin and Credits are on the Account tab.
- **Touch HUD**: the D-pad and the chat/pause buttons are lightly see-through, like 0.9.0. Buttons use 0.9.0's flat style and scale cleanly at any size.

### Phones and tablets in the browser (HTML builds)
- The **touch UI is now the same as in the app**: the hotbar's "..." button opens the inventory, and you tap blocks where your finger is, like 0.9.0.
- **Sizes fit the phone**: the GUI scale follows the screen's short side, the D-pad stays in proportion on 2:1 and wider phones, and the browser now reports a realistic screen density.
- Holding a phone upright shows **"Turn your phone sideways"** (you can choose to play anyway). The installed app locks to landscape where the browser allows it.
- The phone's **Back** button or gesture now works as the game's back button (pause, close menus) instead of leaving the page.
- The screen stays on while you play, and phones buzz when a block breaks (Options -> feedback vibration).
- Tall grass and fern icons are green.

---

## Earlier: MCPE Extended 0.9.5

### New: Java HUD and furnace (PC UI)
- With **Enable PC UI** on (and mouse + keyboard), the in-game HUD is Java-style: the Java hotbar and selection frame, hearts above it and the **armour bar** above the hearts, air bubbles on the right. No hunger or XP bar.
- **Furnaces** open a Java-style furnace screen with input, fuel and output slots, the burning flame and the progress arrow. Shift-click sends smeltable items to the input and fuel to the fuel slot. Works in single player and **on Eaglercraft servers**.

### New: FOV slider
- **Options → Graphics → FOV**, from 30 to 110, **default 90**. Your hand stays at its normal size whatever the FOV.

### Eaglercraft server fixes
- **Item names**: items this game shows as something else keep their real name: Andesite, Granite, Diorite, Coarse Dirt, stained glass colours, carpets, fence gates and so on. Renamed (anvil) items show their name too. You see them in the hotbar name popup and the PC UI tooltips. Server items this game doesn't have at all show as paper with the right name instead of disappearing.
- **No more fake durability bars**: blocks with a data value (andesite, stained glass...) were drawn as damaged items. Blocks never show a durability bar now.
- **Unsupported stairs** (acacia, dark oak, red sandstone, ...) show as **oak stairs** facing the right way instead of stone blocks.
- **Bows work on servers**: releasing the right mouse button now tells the server to shoot.
- **Eating on servers** no longer glitches: the server decides when you can eat, and the client stops the eating animation when the server finishes it.
- Inventory clicks send the server's real item ids, so moving andesite or other stand-in items no longer gets rejected.

### Fixes
- **X-ray with the PC UI inventory open**: drawing the player preview left depth testing off, so the world behind the inventory was drawn see-through. Fixed.
- The browser build's **Fullscreen button is gone**.

---

## Earlier: MCPE Extended 0.9.4

### New: PC UI (Java-style inventory)
- **Options → Controls → Enable PC UI** swaps the Pocket Edition inventory for a Java-style one: armour slots, a player preview that follows your mouse, a 2×2 **Crafting** grid, 27 inventory slots and the hotbar. There's no hunger bar, offhand slot or recipe book.
- Java controls: left click picks up, places, merges or swaps a stack; right click takes half or places one; shift-click moves items between the hotbar and inventory (and crafts as many as fit from the result slot). Hover a slot and press 1–9 to swap with that hotbar slot. Click outside the window to throw the held item. Esc or E closes it.
- Crafting tables open a Java-style 3×3 crafting screen.
- With PC UI on, items you pick up go to the hotbar first, like Java.
- **On Eaglercraft servers** the PC UI sends real Java inventory clicks, so **crafting works on servers** (2×2 and crafting tables). Without PC UI, crafting on a server tells you to turn it on.

### New: PC keys
- **Hold C to zoom** (like the zoom mod; mouse slows down while zoomed).
- **/** opens chat with the slash already typed.
- **F5** cycles first person → third person behind → third person in front. The browser no longer reloads the page when you press F5.

### Fixes
- **Eaglercraft crashes while walking**: entities the game removed on its own (mobs despawning as you walked away, items expiring) stayed referenced and were later used after being freed. Those references are now cleaned up, and mobs on servers only disappear when the server says so.
- **Eaglercraft rubber-banding**: walking into terrain the server hadn't sent yet teleported you back to where you joined. Now you just stop at the edge of loaded terrain until it arrives.
- On servers the hotbar matches Java's (slots 36–44); picking an item into the hotbar in the Pocket Edition inventory swaps it there on the server too.
- **Phones and tablets in the browser (including the single-file HTML)**: tapping a text box now always offers a **"Tap here to type"** bar that opens the keyboard. Phones only open the keyboard from a direct tap.
- **Distant Terrain (LOD) is now off by default.** Turn it on under Options → Graphics.
- The username screen fits on short phone screens.

---

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
