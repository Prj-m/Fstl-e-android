# shellcheck shell=bash
# Sourced by the Android build scripts. arm64-v8a is the primary Qt kit
# (QT_ANDROID_ROOT); other ABIs are built by Qt as sub-projects of that build.
: "${FSTL_ANDROID_ABIS:=arm64-v8a armeabi-v7a x86_64}"
read -r -a fstl_android_abis <<< "${FSTL_ANDROID_ABIS//,/ }"
[[ "${fstl_android_abis[0]:-}" == "arm64-v8a" ]] || { echo "FSTL_ANDROID_ABIS must start with arm64-v8a" >&2; exit 1; }
for abi in "${fstl_android_abis[@]}"; do
    case "$abi" in
        arm64-v8a|armeabi-v7a|x86_64) ;;
        *) echo "Unsupported Android ABI: $abi" >&2; exit 1 ;;
    esac
done

# Compiled viewer library for an ABI inside FSTL_ANDROID_BUILD_DIR.
fstl_abi_library() {
    if [[ "$1" == "${fstl_android_abis[0]}" ]]; then
        echo "$FSTL_ANDROID_BUILD_DIR/libfstl_viewer_$1.so"
    else
        echo "$FSTL_ANDROID_BUILD_DIR/android_abi_builds/$1/libfstl_viewer_$1.so"
    fi
}

# Stage every ABI's viewer library and, for secondary ABIs, its Qt/OCCT
# dependencies into a fresh androiddeployqt output directory ($1).
fstl_stage_abis() {
    local output_dir="$1" abi library
    shift
    for abi in "${fstl_android_abis[@]}"; do
        library="$(fstl_abi_library "$abi")"
        [[ -s "$library" ]] || { echo "Missing compiled $abi viewer library: $library" >&2; exit 1; }
        mkdir -p "$output_dir/libs/$abi"
        cp "$library" "$output_dir/libs/$abi/"
        [[ "$abi" == "${fstl_android_abis[0]}" ]] && continue
        "$ANDROIDDEPLOYQT" --input "$FSTL_ANDROID_BUILD_DIR/android_abi_builds/$abi/android-fstl_viewer-deployment-settings.json" \
            --output "$output_dir" --copy-dependencies-only "$@"
    done
}
