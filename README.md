# fstl-e for Android

<p align="center"><img src="screenshots/android_app_ui_20251125_191127.png" alt="UI screenshot (Android)" width="700"></p>


**Status: Alpha** - Core functionality working, but still in active development.

Android port of [fstl-e](https://github.com/wdaniau/fstl), a fast STL, 3MF, and STEP file viewer.

## Download

**Standard release APK (recommended):**

- [fstl-e-android-arm64-v8a.apk](https://github.com/Prj-m/fstl-e-android/raw/main/dist/fstl-e-android-arm64-v8a.apk)
- [SHA-256 checksum](https://github.com/Prj-m/fstl-e-android/raw/main/dist/fstl-e-android-arm64-v8a.apk.sha256)

**Debug APK (same features, extra logging):**

- [fstl-e-android-debug_20251125_191624.apk](https://github.com/Prj-m/fstl-e-android/raw/main/dist/fstl-e-android-debug_20251125_191624.apk)
- [SHA-256 checksum](https://github.com/Prj-m/fstl-e-android/raw/main/dist/fstl-e-android-debug_20251125_191624.apk.sha256)

## Features

- Fast rendering of STL (binary and ASCII), 3MF, and STEP files
- Multiple draw modes: Shaded, Wireframe, Surface Angle, Meshlight
- Configurable lighting and shader preferences
- Touch gestures: pinch to zoom, drag to rotate
- Auto-reload on file changes
- Displays mesh information (triangle count, dimensions)
- Supports Android's scoped storage (content URIs)

**Note:** STEP file support is experimental and limited to basic geometry. Complex STEP files may not load correctly.

## Building

### Android (lite build, no OCCT)

Requires Qt 6.5+ for Android (tested with Qt 6.10).

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$QT_ROOT/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake ..
cmake --build .
```

### Desktop (with optional OCCT STEP support)

```bash
mkdir build-desktop && cd build-desktop
cmake .. -DENABLE_OCCT_STEP=ON   # assumes OpenCASCADE is installed
cmake --build .
```

When `ENABLE_OCCT_STEP=ON` and OpenCASCADE is found, the viewer will use the
OCCT kernel for STEP files first, and fall back to the internal parser only if
OCCT cannot generate any triangles.

## License

MIT License - see LICENSE file

## Credits

- Original fstl: [Matt Keeter](https://github.com/fstl-app/fstl)
- fstl-e enhancements: [William Daniau](https://github.com/wdaniau/fstl)
- Android port: Prj-m and contributors
