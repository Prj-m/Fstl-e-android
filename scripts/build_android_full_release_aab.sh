#!/usr/bin/env bash
set -euo pipefail

# Build a full STEP-enabled Android release App Bundle (.aab) using the
# existing Qt 6.10 Android build configuration.
#
# This script is designed around the current dev environment layout:
#   - Source:      /path/to/Fstl-e-android
#   - Build dir:   /path/to/AndroidApp/build/Qt_6_10_0_for_Android_arm64_v8a-Debug
#   - Qt Android:  /path/to/Qt/6.10.0/android_arm64_v8a
#   - Qt CMake:    /path/to/Qt/Tools/CMake/bin/cmake
#
# You can override paths via environment variables if needed:
#   FSTL_ANDROID_BUILD_DIR   - CMake build dir (default: Qt_6_10_0_for_Android_arm64_v8a-Debug)
#   FSTL_DEPLOY_JSON         - androiddeployqt deployment JSON
#   ANDROIDDEPLOYQT          - path to androiddeployqt executable
#   QT_CMAKE                 - path to CMake from Qt
#
# Optional signing (manual password entry):
#   If FSTL_SIGN_WITH_KEYSTORE=1 is set, the script will pass --sign
#   arguments to androiddeployqt using:
#     FSTL_KEYSTORE  - path to JKS keystore
#     FSTL_KEY_ALIAS - key alias inside keystore
#   Passwords will NOT be passed on the command line; androiddeployqt
#   will prompt interactively.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${FSTL_ANDROID_BUILD_DIR:=/path/to/AndroidApp/build/Qt_6_10_0_for_Android_arm64_v8a-Debug}"
DEPLOY_JSON_DEFAULT="$FSTL_ANDROID_BUILD_DIR/android-fstl_viewer-deployment-settings.json"
: "${FSTL_DEPLOY_JSON:=$DEPLOY_JSON_DEFAULT}"

QT_CMAKE_DEFAULT="/path/to/Qt/Tools/CMake/bin/cmake"
: "${QT_CMAKE:=$QT_CMAKE_DEFAULT}"

# Try to infer androiddeployqt from the deployment JSON if not provided
if [[ -z "${ANDROIDDEPLOYQT:-}" ]]; then
  if [[ -f "$FSTL_DEPLOY_JSON" ]]; then
    QT_ANDROID_ROOT=$(grep '"arm64-v8a"' "$FSTL_DEPLOY_JSON" | head -n1 | sed -E 's/.*"arm64-v8a"\s*:\s*"([^"]+)".*/\1/')
    ANDROIDDEPLOYQT="$QT_ANDROID_ROOT/bin/androiddeployqt"
  else
    ANDROIDDEPLOYQT="/path/to/Qt/6.10.0/android_arm64_v8a/bin/androiddeployqt"
  fi
fi

if [[ ! -x "$ANDROIDDEPLOYQT" ]]; then
  echo "ERROR: androiddeployqt not found or not executable at: $ANDROIDDEPLOYQT" >&2
  echo "Set ANDROIDDEPLOYQT to the correct path and re-run." >&2
  exit 1
fi

if [[ ! -f "$FSTL_DEPLOY_JSON" ]]; then
  echo "ERROR: Deployment JSON not found: $FSTL_DEPLOY_JSON" >&2
  echo "Make sure the Android CMake build has been configured at: $FSTL_ANDROID_BUILD_DIR" >&2
  exit 1
fi

echo "[1/3] Building native code (fstl_viewer) in: $FSTL_ANDROID_BUILD_DIR"
"$QT_CMAKE" --build "$FSTL_ANDROID_BUILD_DIR" --target fstl_viewer -j"$(nproc)"

OUTPUT_DIR_DEFAULT="$FSTL_ANDROID_BUILD_DIR/android-build-fstl_viewer"
: "${FSTL_ANDROID_OUTPUT_DIR:=$OUTPUT_DIR_DEFAULT}"

mkdir -p "$FSTL_ANDROID_OUTPUT_DIR"

echo "[2/3] Running androiddeployqt to generate release App Bundle (.aab)"

SIGN_ARGS=()
if [[ "${FSTL_SIGN_WITH_KEYSTORE:-0}" == "1" ]]; then
  if [[ -z "${FSTL_KEYSTORE:-}" || -z "${FSTL_KEY_ALIAS:-}" ]]; then
    echo "ERROR: FSTL_SIGN_WITH_KEYSTORE=1 but FSTL_KEYSTORE or FSTL_KEY_ALIAS not set" >&2
    exit 1
  fi
  SIGN_ARGS=("--sign" "$FSTL_KEYSTORE" "$FSTL_KEY_ALIAS")
  echo "Signing enabled. androiddeployqt will prompt for keystore/key passwords."
else
  echo "Signing disabled (FSTL_SIGN_WITH_KEYSTORE not set). The AAB may need to be"
  echo "signed separately or used with Google Play App Signing." 
fi

"$ANDROIDDEPLOYQT" \
  --input  "$FSTL_DEPLOY_JSON" \
  --output "$FSTL_ANDROID_OUTPUT_DIR" \
  --release \
  --aab \
  "${SIGN_ARGS[@]}"

# The resulting AAB is usually under build/outputs/bundle/release/
AAB_PATH=$(find "$FSTL_ANDROID_OUTPUT_DIR" -maxdepth 5 -type f -name "*.aab" | head -n1 || true)

echo "[3/3] Done."
if [[ -n "$AAB_PATH" ]]; then
  echo "Release App Bundle generated at: $AAB_PATH"
else
  echo "WARNING: No .aab file found under $FSTL_ANDROID_OUTPUT_DIR" >&2
fi
