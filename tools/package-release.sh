#!/usr/bin/env bash
# Packages whatever this machine can build into releases/, then rewrites the manifest.
# Linux is not buildable from macOS, that one comes from the release workflow.
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
VER=$(tr -d '[:space:]' < version.txt)
OUT="$ROOT/releases"
mkdir -p "$OUT"

echo "packaging gameboy-emu $VER"

# ---- macos ----------------------------------------------------------------
if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "  macos: building"
    cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release > /dev/null &&
    cmake --build build-release -j"$(sysctl -n hw.ncpu)" > /dev/null
    if [[ -d build-release/gameboy-emu.app ]]; then
        ( cd build-release && ditto -c -k --keepParent --sequesterRsrc \
              gameboy-emu.app "$OUT/gameboy-emu-$VER-macos-arm64.zip" )
        echo "  macos: ok"
    else
        echo "  macos: FAILED"
    fi
fi

# ---- ios, unsigned ipa for sideloading -------------------------------------
if [[ "$(uname -s)" == "Darwin" ]] && command -v xcodebuild > /dev/null; then
    echo "  ios: building (unsigned)"
    cmake -B build-ios-rel -S . -G Xcode -DCMAKE_SYSTEM_NAME=iOS > /dev/null 2>&1
    xcodebuild -project build-ios-rel/gameboy_emu.xcodeproj -target gameboy_emu \
               -configuration Release -sdk iphoneos -arch arm64 \
               CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
               CONFIGURATION_BUILD_DIR="$ROOT/build-ios-rel/out" build > /dev/null 2>&1
    APP="$ROOT/build-ios-rel/out/gameboy-emu.app"
    if [[ -d "$APP" ]]; then
        rm -rf "$ROOT/build-ios-rel/Payload"
        mkdir -p "$ROOT/build-ios-rel/Payload"
        cp -R "$APP" "$ROOT/build-ios-rel/Payload/"
        ( cd "$ROOT/build-ios-rel" && zip -qry "$OUT/gameboy-emu-$VER-ios-unsigned.ipa" Payload )
        echo "  ios: ok"
    else
        echo "  ios: FAILED (no .app produced)"
    fi
fi

# ---- android ---------------------------------------------------------------
# a release apk is only usable if it was actually signed. gradle happily emits
# app-release-unsigned.apk when no passwords are configured, and android refuses to
# install that, so it is never shipped
echo "  android: building"
APK=""
if ( cd android && ./gradlew assembleRelease > /dev/null 2>&1 ); then
    APK=$(ls android/app/build/outputs/apk/release/*.apk 2>/dev/null | grep -v unsigned | head -1)
fi
if [[ -n "$APK" ]]; then
    cp "$APK" "$OUT/gameboy-emu-$VER-android.apk"
    echo "  android: ok (release key)"
elif ( cd android && ./gradlew assembleDebug > /dev/null 2>&1 ) && \
     APK=$(ls android/app/build/outputs/apk/debug/*.apk 2>/dev/null | head -1) && [[ -n "${APK:-}" ]]; then
    cp "$APK" "$OUT/gameboy-emu-$VER-android.apk"
    echo "  android: ok (DEBUG key, set android/keystore.properties to sign properly)"
else
    echo "  android: FAILED"
fi

python3 "$ROOT/tools/write-manifest.py"
