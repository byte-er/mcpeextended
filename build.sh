#!/usr/bin/env bash
# ============================================================
#  MCPE Extended - Android APK build script (Linux / macOS / CI)
#
#  Requirements:
#    ANDROID_NDK_HOME  -> Android NDK r26+ (tested with r27d)
#    ANDROID_HOME      -> Android SDK with build-tools and a platform
#    JDK 11+           -> javac / keytool on PATH
#
#  Usage:
#    ./build.sh                 # build native libs + java + signed APK
#    ./build.sh --no-cpp        # skip the NDK build (reuse existing .so)
#    ./build.sh --abi arm64-v8a # build a single ABI (default: arm64-v8a armeabi-v7a)
#    ./build.sh --install       # adb install the result afterwards
#
#  Output: out/apk/mcpeextended-<version>-<abi>.apk
# ============================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NO_CPP=0
INSTALL=0
ABIS="arm64-v8a armeabi-v7a"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-cpp)  NO_CPP=1 ;;
        --install) INSTALL=1 ;;
        --abi)     shift; ABIS="$1" ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "unknown option $1"; exit 1 ;;
    esac
    shift
done

: "${ANDROID_HOME:=${ANDROID_SDK_ROOT:-/opt/android}}"
: "${ANDROID_NDK_HOME:=${ANDROID_NDK_ROOT:-$(ls -d "$ANDROID_HOME"/ndk/* "$ANDROID_HOME"/android-ndk-* 2>/dev/null | sort -V | tail -1)}}"

BT_DIR="$(ls -d "$ANDROID_HOME"/build-tools/* | sort -V | tail -1)"
PLATFORM_DIR="$(ls -d "$ANDROID_HOME"/platforms/android-* | sort -V | tail -1)"
ANDROID_JAR="$PLATFORM_DIR/android.jar"

OUT="$REPO/out"
WORK="$OUT/work"
APK_OUT="$OUT/apk"
MANIFEST="$REPO/project/android_java/AndroidManifest.xml"
RES="$REPO/project/android_java/res"
JAVA_SRC="$REPO/project/android_java/src"
KEYSTORE="${KEYSTORE:-$OUT/debug.keystore}"
KS_PASS="${KS_PASS:-android}"
KEY_ALIAS="${KEY_ALIAS:-androiddebugkey}"
VERSION="$(sed -n 's/.*android:versionName="\([^"]*\)".*/\1/p' "$MANIFEST")"

step() { printf '\n\033[36m==> %s\033[0m\n' "$*"; }

echo "NDK:         $ANDROID_NDK_HOME"
echo "build-tools: $BT_DIR"
echo "platform:    $PLATFORM_DIR"
echo "ABIs:        $ABIS"
echo "version:     $VERSION"

mkdir -p "$WORK" "$APK_OUT"

# ── 0. Keystore ─────────────────────────────────────────────
if [[ ! -f "$KEYSTORE" ]]; then
    step "Generating debug keystore"
    keytool -genkeypair -keystore "$KEYSTORE" -storepass "$KS_PASS" -keypass "$KS_PASS" \
        -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
fi

# ── 1. Native build ─────────────────────────────────────────
if [[ $NO_CPP -eq 0 ]]; then
    step "NDK build ($ABIS)"
    "$ANDROID_NDK_HOME/ndk-build" -C "$REPO/project/android" -j"$(nproc 2>/dev/null || echo 4)" \
        NDK_PROJECT_PATH=. \
        APP_BUILD_SCRIPT=jni/Android.mk \
        NDK_APPLICATION_MK=jni/Application.mk \
        APP_ABI="$ABIS" \
        NDK_OUT="$WORK/obj" \
        NDK_LIBS_OUT="$WORK/libs"
fi

# ── 2. Java compile ─────────────────────────────────────────
step "Java compile"
rm -rf "$WORK/gen" "$WORK/classes" "$WORK/dex"
mkdir -p "$WORK/gen" "$WORK/classes" "$WORK/dex"
"$BT_DIR/aapt" package -f -m -M "$MANIFEST" -S "$RES" -I "$ANDROID_JAR" -J "$WORK/gen"
find "$JAVA_SRC" "$WORK/gen" -name '*.java' > "$WORK/sources.txt"
javac -nowarn --release 8 -encoding UTF-8 -cp "$ANDROID_JAR" -d "$WORK/classes" @"$WORK/sources.txt" 2>&1 \
    | grep -v -E '^(warning|Note|1 warning)' || true
[[ -n "$(find "$WORK/classes" -name '*.class' -print -quit)" ]] || { echo "javac failed"; exit 1; }
"$BT_DIR/d8" --release --min-api 21 --lib "$ANDROID_JAR" --output "$WORK/dex" $(find "$WORK/classes" -name '*.class')

# ── 3. Package one APK per ABI + a universal APK ────────────
package() {
    local name="$1"; shift
    local abis="$*"
    local stage="$WORK/stage-$name"
    local unsigned="$WORK/$name-unsigned.apk"
    local aligned="$WORK/$name-aligned.apk"
    local final="$APK_OUT/mcpeextended-$VERSION-$name.apk"
    step "Package $name"
    rm -rf "$stage" "$unsigned" "$aligned" "$final"
    mkdir -p "$stage/assets"
    cp -r "$REPO/data/." "$stage/assets/"
    # strip desktop-only / unused bulk data from the APK
    rm -rf "$stage/assets/app/ios" "$stage/assets/images/terrain.pvr"* "$stage/assets/images/"*.h
    for abi in $abis; do
        mkdir -p "$stage/lib/$abi"
        cp "$WORK/libs/$abi/libminecraftpe.so" "$stage/lib/$abi/"
    done
    cp "$WORK/dex/classes.dex" "$stage/"
    "$BT_DIR/aapt" package -f -M "$MANIFEST" -S "$RES" -I "$ANDROID_JAR" -F "$unsigned"
    # .so and assets stored uncompressed (page-aligned by zipalign -p); dex compressed
    (cd "$stage" && zip -q -0 -r "$unsigned" lib assets && zip -q -9 "$unsigned" classes.dex)
    "$BT_DIR/zipalign" -f -p 4 "$unsigned" "$aligned"
    "$BT_DIR/apksigner" sign --ks "$KEYSTORE" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KS_PASS" \
        --ks-key-alias "$KEY_ALIAS" --v4-signing-enabled false --out "$final" "$aligned"
    "$BT_DIR/apksigner" verify "$final"
    echo "  -> $final ($(du -h "$final" | cut -f1))"
}

for abi in $ABIS; do package "$abi" "$abi"; done
n_abis=$(wc -w <<<"$ABIS")
if [[ $n_abis -gt 1 ]]; then package universal $ABIS; fi

# ── 4. Optional install ─────────────────────────────────────
if [[ $INSTALL -eq 1 ]]; then
    step "Install"
    adb install -r "$APK_OUT/mcpeextended-$VERSION-$( [[ $n_abis -gt 1 ]] && echo universal || echo "$ABIS").apk"
fi

step "Done"
ls -la "$APK_OUT"
