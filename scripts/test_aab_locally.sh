#!/usr/bin/env bash
set -euo pipefail

# Test AAB locally by generating APKs and installing them on connected device
# This simulates what Play Store does, so you can test locally before uploading

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

AAB_PATH="${1:-/path/to/AndroidApp/build/Qt_6_10_0_for_Android_arm64_v8a-Debug/android-build-fstl_viewer/build/outputs/bundle/release/android-build-fstl_viewer-release.aab}"
BUNDLETOOL="${BUNDLETOOL:-$HOME/bundletool.jar}"
OUTPUT_DIR="${2:-$REPO_ROOT/dist}"

if [[ ! -f "$AAB_PATH" ]]; then
    echo "ERROR: AAB not found at: $AAB_PATH"
    echo "Usage: $0 [aab_path] [output_dir]"
    exit 1
fi

# Download bundletool if not present
if [[ ! -f "$BUNDLETOOL" ]]; then
    echo "Downloading bundletool..."
    curl -L -o "$BUNDLETOOL" \
        https://github.com/google/bundletool/releases/latest/download/bundletool-all.jar
fi

mkdir -p "$OUTPUT_DIR"
APKS_PATH="$OUTPUT_DIR/app.apks"

echo "Building APKs from AAB..."
java -jar "$BUNDLETOOL" build-apks \
    --bundle="$AAB_PATH" \
    --output="$APKS_PATH" \
    --mode=universal

echo "Installing APKs to device..."
java -jar "$BUNDLETOOL" install-apks \
    --apks="$APKS_PATH"

echo ""
echo "App installed! Launch it on your device to test."
echo "To uninstall: adb uninstall com.github.prjm.fstl_e"
