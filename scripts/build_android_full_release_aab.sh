#!/usr/bin/env bash
set -euo pipefail

# Build full STEP support from explicit SDK/dependency paths. See docs/CI_CD.md.
if [[ "${1:-}" == "--help" ]]; then
    cat <<'HELP'
Required: QT_ANDROID_ROOT (arm64 kit), ANDROID_SDK_ROOT, ANDROID_NDK_ROOT, FSTL_OCCT_ROOT (arm64)
ABIs:     FSTL_ANDROID_ABIS (default "arm64-v8a armeabi-v7a x86_64"). Secondary ABIs
          need sibling Qt kits (android_armv7, android_x86_64) and OCCT prefixes
          FSTL_OCCT_ROOT_ARMEABI_V7A / FSTL_OCCT_ROOT_X86_64 (default $FSTL_OCCT_ROOT-<abi>).
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
# shellcheck source=scripts/android_abis.sh
source "$repo_root/scripts/android_abis.sh"

command -v "$QT_CMAKE" >/dev/null || fail "CMake not found: $QT_CMAKE"
command -v ninja >/dev/null || fail "Ninja is required"
[[ -x "$ANDROIDDEPLOYQT" ]] || fail "androiddeployqt not executable: $ANDROIDDEPLOYQT"
[[ -f "$QT_ANDROID_ROOT/lib/cmake/Qt6/qt.toolchain.cmake" ]] || fail "Invalid Qt Android kit"
[[ -f "$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" ]] || fail "Invalid Android NDK"
[[ -d "$ANDROID_SDK_ROOT/platforms/android-36" ]] || fail "Install Android SDK platform 36"
[[ -f "$FSTL_OCCT_ROOT/lib/libTKDESTEP.so" ]] || fail "Android OCCT STEP library is missing"
[[ -f "$FSTL_OCCT_ROOT/include/opencascade/STEPControl_Reader.hxx" ]] || fail "OCCT headers are missing"
[[ "$FSTL_BUILD_JOBS" =~ ^[1-9][0-9]*$ ]] || fail "FSTL_BUILD_JOBS must be a positive integer"

abi_args=()
forward_vars="ENABLE_OCCT_STEP;FSTL_REQUIRE_OCCT;FSTL_ANDROID_TARGET_SDK;ANDROID_PLATFORM;ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES;CMAKE_C_FLAGS;CMAKE_CXX_FLAGS"
prefix_map_flags="-ffile-prefix-map=$repo_root=/src/fstl-e -ffile-prefix-map=$QT_ANDROID_ROOT=/deps/qt -ffile-prefix-map=$FSTL_OCCT_ROOT=/deps/occt"
for abi in "${fstl_android_abis[@]:1}"; do
    abi_var="FSTL_OCCT_ROOT_$(tr 'a-z-' 'A-Z_' <<< "$abi")"
    occt_root="${!abi_var:-$FSTL_OCCT_ROOT-$abi}"
    [[ -f "$occt_root/lib/libTKDESTEP.so" ]] || fail "OCCT for $abi is missing; set $abi_var"
    case "$abi" in armeabi-v7a) kit=android_armv7 ;; *) kit="android_$abi" ;; esac
    qt_kit="$(dirname "$QT_ANDROID_ROOT")/$kit"
    [[ -f "$qt_kit/lib/cmake/Qt6/qt.toolchain.cmake" ]] || fail "Qt Android kit for $abi is missing: $qt_kit"
    abi_args+=("-DFSTL_OCCT_ROOT_$abi=$occt_root" "-DQT_PATH_ANDROID_ABI_$abi=$qt_kit")
    forward_vars+=";FSTL_OCCT_ROOT_$abi"
    prefix_map_flags+=" -ffile-prefix-map=$qt_kit=/deps/qt -ffile-prefix-map=$occt_root=/deps/occt"
done

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
    -DCMAKE_C_FLAGS="$prefix_map_flags" -DCMAKE_CXX_FLAGS="$prefix_map_flags" \
    -DCMAKE_BUILD_TYPE=Release -DFSTL_ANDROID_TARGET_SDK=36 \
    -DENABLE_OCCT_STEP=ON -DFSTL_REQUIRE_OCCT=ON \
    -DFSTL_OCCT_ROOT="$FSTL_OCCT_ROOT" \
    -DFSTL_ANDROID_ABIS="$(IFS=";"; echo "${fstl_android_abis[*]}")" \
    -DQT_ANDROID_MULTI_ABI_FORWARD_VARS="$forward_vars" "${abi_args[@]}"
build_targets=(fstl_viewer)
for abi in "${fstl_android_abis[@]:1}"; do
    build_targets+=("qt_internal_android_${abi}_fstl_viewer_build")
done
"$QT_CMAKE" --build "$FSTL_ANDROID_BUILD_DIR" --target "${build_targets[@]}" --parallel "$FSTL_BUILD_JOBS"

deploy_json="$FSTL_ANDROID_BUILD_DIR/android-fstl_viewer-deployment-settings.json"
[[ -s "$deploy_json" ]] || fail "Qt deployment settings were not generated"
# A new packaging directory prevents an old AAB from passing artifact checks.
output_dir="$(mktemp -d "$FSTL_ANDROID_BUILD_DIR/android-release.XXXXXX")"
fstl_stage_abis "$output_dir" --release
"$ANDROIDDEPLOYQT" --input "$deploy_json" --output "$output_dir" --release --aab "${sign_args[@]}"
[[ -f "$output_dir/gradlew" ]] || fail "Qt did not generate a Gradle wrapper"
(cd "$output_dir" && bash ./gradlew --no-daemon lintRelease)

mapfile -t bundles < <(find "$output_dir" -type f -path '*/outputs/bundle/release/*.aab')
[[ "${#bundles[@]}" == 1 && -s "${bundles[0]}" ]] || fail "Expected exactly one nonempty release AAB"
python3 "$repo_root/scripts/verify_android_bundle.py" --abis "${fstl_android_abis[*]}" "${bundles[0]}"
artifact_dir="$FSTL_ANDROID_BUILD_DIR/artifacts"
mkdir -p "$artifact_dir"
cp "${bundles[0]}" "$artifact_dir/fstl-e-release.aab"
(cd "$artifact_dir" && sha256sum fstl-e-release.aab > fstl-e-release.aab.sha256)
echo "Validated release bundle: $artifact_dir/fstl-e-release.aab"
