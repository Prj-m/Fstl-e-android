#!/usr/bin/env bash
set -euo pipefail

# Build full STEP support from explicit SDK/dependency paths. See docs/CI_CD.md.
if [[ "${1:-}" == "--help" ]]; then
    cat <<'HELP'
Required: QT_ANDROID_ROOT, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT, FSTL_OCCT_ROOT
Optional: QT_HOST_ROOT (defaults to sibling gcc_64), QT_CMAKE (defaults to cmake),
          FSTL_ANDROID_BUILD_DIR, ANDROIDDEPLOYQT, FSTL_BUILD_JOBS
Signing:  FSTL_SIGN_WITH_KEYSTORE=1 requires FSTL_KEYSTORE and FSTL_KEY_ALIAS.
          Passwords are prompted interactively, never passed as command arguments.
          Default is 0: produce an unsigned AAB for validation, not Play upload.
HELP
    exit 0
fi

fail() { echo "ERROR: $*" >&2; exit 1; }
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${QT_ANDROID_ROOT:?Set QT_ANDROID_ROOT to the Qt Android arm64 kit}"
: "${ANDROID_SDK_ROOT:?Set ANDROID_SDK_ROOT}"
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT}"
: "${FSTL_OCCT_ROOT:?Set FSTL_OCCT_ROOT to the Android OCCT installation}"
: "${QT_HOST_ROOT:=$(dirname "$QT_ANDROID_ROOT")/gcc_64}"
: "${QT_CMAKE:=cmake}"
: "${ANDROIDDEPLOYQT:=$QT_HOST_ROOT/bin/androiddeployqt}"
: "${FSTL_ANDROID_BUILD_DIR:=$repo_root/build/android-release}"
: "${FSTL_BUILD_JOBS:=2}"
: "${FSTL_SIGN_WITH_KEYSTORE:=0}"

command -v "$QT_CMAKE" >/dev/null || fail "CMake not found: $QT_CMAKE"
command -v ninja >/dev/null || fail "Ninja is required"
[[ -x "$ANDROIDDEPLOYQT" ]] || fail "androiddeployqt not executable: $ANDROIDDEPLOYQT"
[[ -f "$QT_ANDROID_ROOT/lib/cmake/Qt6/qt.toolchain.cmake" ]] || fail "Invalid Qt Android kit"
[[ -f "$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" ]] || fail "Invalid Android NDK"
[[ -d "$ANDROID_SDK_ROOT/platforms/android-36" ]] || fail "Install Android SDK platform 36"
[[ -f "$FSTL_OCCT_ROOT/lib/libTKDESTEP.so" ]] || fail "Android OCCT STEP library is missing"
[[ -f "$FSTL_OCCT_ROOT/include/opencascade/STEPControl_Reader.hxx" ]] || fail "OCCT headers are missing"
[[ "$FSTL_BUILD_JOBS" =~ ^[1-9][0-9]*$ ]] || fail "FSTL_BUILD_JOBS must be a positive integer"

sign_args=()
case "$FSTL_SIGN_WITH_KEYSTORE" in
    0) echo "Building unsigned validation bundle." ;;
    1)
        [[ -n "${FSTL_KEYSTORE:-}" && -n "${FSTL_KEY_ALIAS:-}" ]] || fail "Set FSTL_KEYSTORE and FSTL_KEY_ALIAS"
        [[ -f "$FSTL_KEYSTORE" ]] || fail "Signing keystore is missing"
        sign_args=(--sign "$FSTL_KEYSTORE" "$FSTL_KEY_ALIAS")
        ;;
    *) fail "FSTL_SIGN_WITH_KEYSTORE must be 0 or 1" ;;
esac

"$QT_CMAKE" -S "$repo_root" -B "$FSTL_ANDROID_BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$QT_ANDROID_ROOT/lib/cmake/Qt6/qt.toolchain.cmake" \
    -DQT_HOST_PATH="$QT_HOST_ROOT" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
    -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
    -DCMAKE_BUILD_TYPE=Release -DFSTL_ANDROID_TARGET_SDK=36 \
    -DENABLE_OCCT_STEP=ON -DFSTL_REQUIRE_OCCT=ON \
    -DFSTL_OCCT_ROOT="$FSTL_OCCT_ROOT"
"$QT_CMAKE" --build "$FSTL_ANDROID_BUILD_DIR" --target fstl_viewer --parallel "$FSTL_BUILD_JOBS"

deploy_json="$FSTL_ANDROID_BUILD_DIR/android-fstl_viewer-deployment-settings.json"
[[ -s "$deploy_json" ]] || fail "Qt deployment settings were not generated"
# A new packaging directory prevents an old AAB from passing artifact checks.
output_dir="$(mktemp -d "$FSTL_ANDROID_BUILD_DIR/android-release.XXXXXX")"
native_library="$FSTL_ANDROID_BUILD_DIR/libfstl_viewer_arm64-v8a.so"
[[ -s "$native_library" ]] || { echo "Missing compiled arm64 viewer library" >&2; exit 1; }
mkdir -p "$output_dir/libs/arm64-v8a"
cp "$native_library" "$output_dir/libs/arm64-v8a/"
"$ANDROIDDEPLOYQT" --input "$deploy_json" --output "$output_dir" --release --aab "${sign_args[@]}"
[[ -f "$output_dir/gradlew" ]] || fail "Qt did not generate a Gradle wrapper"
(cd "$output_dir" && bash ./gradlew --no-daemon lintRelease)

mapfile -t bundles < <(find "$output_dir" -type f -path '*/outputs/bundle/release/*.aab')
[[ "${#bundles[@]}" == 1 && -s "${bundles[0]}" ]] || fail "Expected exactly one nonempty release AAB"
python3 "$repo_root/scripts/verify_android_bundle.py" "${bundles[0]}"
artifact_dir="$FSTL_ANDROID_BUILD_DIR/artifacts"
mkdir -p "$artifact_dir"
cp "${bundles[0]}" "$artifact_dir/fstl-e-arm64-release.aab"
(cd "$artifact_dir" && sha256sum fstl-e-arm64-release.aab > fstl-e-arm64-release.aab.sha256)
echo "Validated release bundle: $artifact_dir/fstl-e-arm64-release.aab"
