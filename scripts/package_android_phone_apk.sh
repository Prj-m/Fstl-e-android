#!/usr/bin/env bash
set -euo pipefail

# Reuse the compiled full-STEP native build; package separately from release.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${QT_ANDROID_ROOT:?Set QT_ANDROID_ROOT}"
: "${ANDROID_SDK_ROOT:?Set ANDROID_SDK_ROOT}"
: "${QT_HOST_ROOT:=$(dirname "$QT_ANDROID_ROOT")/gcc_64}"
: "${ANDROIDDEPLOYQT:=$QT_HOST_ROOT/bin/androiddeployqt}"
: "${FSTL_ANDROID_BUILD_DIR:=$repo_root/build/android-release}"
# shellcheck source=scripts/android_abis.sh
source "$repo_root/scripts/android_abis.sh"
deploy_json="$FSTL_ANDROID_BUILD_DIR/android-fstl_viewer-deployment-settings.json"
[[ -s "$deploy_json" ]] || { echo "Run build_android_full_release_aab.sh first" >&2; exit 1; }
output_dir="$(mktemp -d "$FSTL_ANDROID_BUILD_DIR/android-phone.XXXXXX")"
# Default Qt packaging builds Debug: uses the .dev application ID and a test key.
fstl_stage_abis "$output_dir"
"$ANDROIDDEPLOYQT" --input "$deploy_json" --output "$output_dir"
(cd "$output_dir" && bash ./gradlew --no-daemon lintDebug)
mapfile -t apks < <(find "$output_dir" -type f -path '*/outputs/apk/debug/*.apk')
[[ "${#apks[@]}" == 1 && -s "${apks[0]}" ]] || { echo "Expected one fresh debug APK" >&2; exit 1; }
python3 "$repo_root/scripts/verify_android_bundle.py" --abis "${fstl_android_abis[*]}" --apk "${apks[0]}"
sdk_tools="$ANDROID_SDK_ROOT/build-tools/36.0.0"
"$sdk_tools/apksigner" verify "${apks[0]}"
"$sdk_tools/zipalign" -c -P 16 -v 4 "${apks[0]}"
# Ensure the APK cannot replace the user's installed release app.
"$sdk_tools/aapt" dump badging "${apks[0]}" | python3 -c '
import sys
if "package: name=\x27com.github.prjm.fstl_e.dev\x27" not in sys.stdin.read():
    sys.exit("Phone APK must use the separate .dev application ID")
'
artifact_dir="$FSTL_ANDROID_BUILD_DIR/phone-artifacts"
mkdir -p "$artifact_dir"
cp "${apks[0]}" "$artifact_dir/fstl-e-dev.apk"
(cd "$artifact_dir" && sha256sum fstl-e-dev.apk > fstl-e-dev.apk.sha256)
echo "Phone test APK: $artifact_dir/fstl-e-dev.apk"
