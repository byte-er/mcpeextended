# mcpe64

> [!WARNING]  
> This repository has been merged with the original repository and is **no longer actively maintained**.  
> Future work will continue on the original repo: https://gitea.sffempire.ru/Kolyah35/minecraft-pe-0.6.1  
> Join the Discord server for updates and support: https://discord.gg/ryZ884DWJf

Source code for **Minecraft Pocket Edition 0.6.1 alpha** with various fixes and improvements.

This project builds upon the original repository by Kolyah35 and aims to preserve and improve this early version of Minecraft PE.

I've done some bad and pointless changes to the code so if someone could improve my horrible debugging skills I'd be very happy.

## Roadmap
- [x] Fix fog
- [x] Fix sound
- [x] Add sprinting
- [x] Chat (semi working) and commands
- [x] Implementing options 
- [ ] Controller support
- [ ] Minecraft server hosting
- [x] Android build support
    - [x] Touch control improvements
    - [ ] Screen fixes
- [x] Better F3
- [ ] Performance optimalizations

## What's new in this fork (0.9.0 update)
See [RELEASE_NOTES.md](RELEASE_NOTES.md). Highlights: Java Edition world border (`/worldborder`),
Java style view bobbing, 512×512 worlds, Pocket Edition 0.9.0 blocks and world generation,
floating point precision fixes, compressed saves and a round of stability fixes.

## How to Build
### Android (Linux / macOS / CI)
Needs an Android SDK (build-tools + a platform), NDK r26 or newer (tested with r27d) and a JDK 11+.
```
export ANDROID_HOME=/path/to/sdk
export ANDROID_NDK_HOME=/path/to/android-ndk-r27d
./build.sh                  # arm64-v8a + armeabi-v7a + universal APKs in out/apk/
./build.sh --abi arm64-v8a  # single ABI
./build.sh --no-cpp         # repackage without recompiling C++
./build.sh --install        # adb install afterwards
```
APKs are signed with a generated debug key unless `KEYSTORE`, `KS_PASS` and `KEY_ALIAS` are set.

GitHub Actions (`.github/workflows/build.yml`) builds Android, Windows, Linux and web on every push. Pushing a `v*` tag,
running the workflow manually, or a commit message containing `[release]` publishes a GitHub release.
Add `KEYSTORE_BASE64`, `KEYSTORE_PASSWORD` and `KEY_ALIAS` repository secrets to sign releases with a
stable key (otherwise each release is signed with a throwaway debug key).

### Android (Windows)
Download an Android NDK and run `build.ps1`:
```
# Full build (NDK + Java + APK + install)
C:\apkbuild\build.ps1

# Skip NDK recompile (Java/assets changed only)
C:\apkbuild\build.ps1 -NoCpp

# Skip Java recompile (C++ changed only)
C:\apkbuild\build.ps1 -NoJava

# Only repackage + install (no recompile at all)
C:\apkbuild\build.ps1 -NoBuild
```

### Browser / PWA (also the iOS route)
Needs the Emscripten SDK (tested with 3.1.74):
```
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web
cp web/manifest.webmanifest web/sw.js web/icon-*.png build-web/
python3 -m http.server -d build-web 8000   # open http://localhost:8000
```
Saves go to IndexedDB. Serve over HTTPS to install it as an app (on iOS: Safari, Share, Add to Home Screen).
The CI can also publish it to GitHub Pages: enable Pages with source "GitHub Actions" and set the
repository variable `ENABLE_PAGES=true`.

### Windows (cross compiled from Linux)
```
sudo apt install mingw-w64
cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake && cmake --build build-win
```

### Desktop (Linux / Windows)
```
cmake -B build -G Ninja && cmake --build build
./build/MinecraftPE
```
For testing, `MCPE_AUTOWORLD=<name> [MCPE_SEED=<n>] [MCPE_CREATIVE=1]` loads or creates a world straight away.

## Credits
New block textures come from the "Old Default 1.13.2" resource pack.

Original repository:  
https://gitea.sffempire.ru/Kolyah35/minecraft-pe-0.6.1
