#!/usr/bin/env bash
# Builds MothBite (VST3 + AU + Standalone) and flattens the results into build/VST3,
# build/AU and build/Standalone. COPY_PLUGIN_AFTER_BUILD in CMakeLists.txt already installs
# the VST3/AU into the system plugin folders (~/Library/Audio/Plug-Ins/...), so a DAW rescan
# is all that's needed afterwards - no manual copying required for day-to-day use.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

CONFIG="Release"
BUILD_DIR="build"

echo "==> Configuring ($BUILD_DIR/)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" > /dev/null

echo "==> Building VST3 + AU + Standalone ($CONFIG)..."
cmake --build "$BUILD_DIR" --config "$CONFIG" \
    --target MothBite_VST3 MothBite_AU MothBite_Standalone \
    -j "$(sysctl -n hw.ncpu)"

# CMakeLists.txt redirects MothBite's own output to build/<Config>/<Format>/... (flattened one
# level from JUCE's default, via the RUNTIME/LIBRARY/ARCHIVE_OUTPUT_DIRECTORY override) - copy
# those into the final build/{VST3,AU,Standalone}/ locations.
ARTEFACTS="$BUILD_DIR/$CONFIG"

FLAT_VST3_DIR="$BUILD_DIR/VST3"
FLAT_AU_DIR="$BUILD_DIR/AU"
FLAT_STANDALONE_DIR="$BUILD_DIR/Standalone"

rm -rf "$FLAT_VST3_DIR" "$FLAT_AU_DIR" "$FLAT_STANDALONE_DIR"
mkdir -p "$FLAT_VST3_DIR" "$FLAT_AU_DIR" "$FLAT_STANDALONE_DIR"

cp -R "$ARTEFACTS/VST3/MothBite.vst3" "$FLAT_VST3_DIR/"
cp -R "$ARTEFACTS/AU/MothBite.component" "$FLAT_AU_DIR/"
cp -R "$ARTEFACTS/Standalone/MothBite.app" "$FLAT_STANDALONE_DIR/"

echo
echo "==> Build complete."
echo "    VST3:       $FLAT_VST3_DIR/MothBite.vst3"
echo "    AU:         $FLAT_AU_DIR/MothBite.component"
echo "    Standalone: $FLAT_STANDALONE_DIR/MothBite.app"
echo
echo "    Also installed system-wide (COPY_PLUGIN_AFTER_BUILD):"
echo "    ~/Library/Audio/Plug-Ins/VST3/MothBite.vst3"
echo "    ~/Library/Audio/Plug-Ins/Components/MothBite.component"
