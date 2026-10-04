#!/usr/bin/env bash
set -euo pipefail
# OCCT 7.9.3; source is obtained by the workflow at an immutable commit.
: "${FSTL_OCCT_SOURCE:?Set FSTL_OCCT_SOURCE to the OCCT source checkout}"
: "${FSTL_OCCT_ROOT:?Set FSTL_OCCT_ROOT to the installation prefix}"
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT}"
: "${FSTL_BUILD_JOBS:=2}"
occt_build_dir="${FSTL_OCCT_BUILD_DIR:-$FSTL_OCCT_ROOT-build}"
cmake -S "$FSTL_OCCT_SOURCE" -B "$occt_build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
    -DANDROID_STL=c++_shared -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$FSTL_OCCT_ROOT" \
    -DINSTALL_DIR_LAYOUT=Unix -DBUILD_LIBRARY_TYPE=Shared -DBUILD_SOVERSION_NUMBERS=0 \
    -DBUILD_RELEASE_DISABLE_EXCEPTIONS=OFF \
    -DBUILD_MODULE_FoundationClasses=OFF -DBUILD_MODULE_ModelingData=OFF \
    -DBUILD_MODULE_ModelingAlgorithms=OFF -DBUILD_MODULE_Visualization=OFF \
    -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_DataExchange=OFF \
    -DBUILD_MODULE_DETools=OFF -DBUILD_MODULE_Draw=OFF \
    -DBUILD_ADDITIONAL_TOOLKITS='TKDESTEP;TKMesh' \
    -DUSE_TK=OFF -DUSE_FREETYPE=OFF -DUSE_TBB=OFF -DUSE_RAPIDJSON=OFF \
    -DUSE_DRACO=OFF -DUSE_EIGEN=OFF
cmake --build "$occt_build_dir" --parallel "$FSTL_BUILD_JOBS"
cmake --install "$occt_build_dir"
