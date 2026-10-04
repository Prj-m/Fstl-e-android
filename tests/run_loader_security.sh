#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
if [[ -n "${QT_ROOT_DIR:-}" ]]; then
    export PKG_CONFIG_PATH="$QT_ROOT_DIR/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi
qt_headers="$(pkg-config --variable=includedir Qt6Core)"
qt_version="$(pkg-config --modversion Qt6Core)"
qmake_command="${QMAKE:-qmake6}"
if [[ -z "${QMAKE:-}" && -n "${QT_ROOT_DIR:-}" && -x "$QT_ROOT_DIR/bin/qmake" ]]; then
    qmake_command="$QT_ROOT_DIR/bin/qmake"
fi
if ! command -v "$qmake_command" >/dev/null && [[ -z "${QMAKE:-}" ]]; then
    qmake_command=qmake
fi
qt_libexec="$("$qmake_command" -query QT_HOST_LIBEXECS)"
"$qt_libexec/moc" "$root/include/core/loader.h" -o "$build_dir/moc_loader.cpp"
read -r -a qt_flags <<< "$(pkg-config --cflags --libs Qt6Core Qt6Gui Qt6OpenGL)"
"${CXX:-g++}" -std=c++17 -fPIC -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$root/include" -I"$qt_headers/QtCore/$qt_version" \
    -I"$qt_headers/QtCore/$qt_version/QtCore" \
    "$root/tests/loader_security.cpp" "$root/src/core/loader.cpp" \
    "$root/src/core/mesh.cpp" "$root/src/loaders/stepmeshloader.cpp" \
    "$root/src/loaders/occtsteploader.cpp" "$build_dir/moc_loader.cpp" \
    "${qt_flags[@]}" -pthread -o "$build_dir/loader_security"
# LeakSanitizer cannot run under ptrace in some managed workspaces.
# Address and undefined-behavior checks remain enabled.
ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}" "$build_dir/loader_security"
