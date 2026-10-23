#!/usr/bin/env bash
set -euo pipefail
[[ "$#" == 1 && -s "$1" ]] || { echo "Usage: $0 <release.aab>" >&2; exit 1; }
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/android_abis.sh
source "$repo_root/scripts/android_abis.sh"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
# Immutable digest verified against Google's GitHub release asset metadata.
curl --fail --location --silent --show-error \
    https://github.com/google/bundletool/releases/download/1.18.3/bundletool-all-1.18.3.jar \
    --output "$work_dir/bundletool.jar"
echo "a099cfa1543f55593bc2ed16a70a7c67fe54b1747bb7301f37fdfd6d91028e29  $work_dir/bundletool.jar" | sha256sum --check --status
java -jar "$work_dir/bundletool.jar" validate --bundle="$1"
java -jar "$work_dir/bundletool.jar" dump manifest --bundle="$1" --module=base > "$work_dir/manifest.xml"
version_args=()
if [[ -n "${FSTL_PLAY_HIGHEST_VERSION_CODE:-}" ]]; then
    version_args=(--highest-version-code "$FSTL_PLAY_HIGHEST_VERSION_CODE")
fi
python3 "$repo_root/scripts/verify_android_manifest.py" "$work_dir/manifest.xml" "${version_args[@]}"
python3 "$repo_root/scripts/verify_android_bundle.py" --abis "${fstl_android_abis[*]}" "$1"
