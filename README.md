# fstl-e for Android

<p align="center"><img src="screenshots/android_app_ui_20251125_191127.png" alt="UI screenshot (Android)" width="600"></p>


**Status: Beta — release readiness under review.**
Android port of [fstl-e](https://github.com/wdaniau/fstl), a fast STL, 3MF, and STEP file viewer.

## Download

**Google Play:**

[App listing](https://play.google.com/store/apps/details?id=com.github.prjm.fstl_e) — current track and public availability still require Play Console verification.

**Direct APK Downloads:**

For users who prefer sideloading or don't have access to Google Play:

- **ARM64 APK** (v1.0.3, ~40MB): [Download](https://github.com/Prj-m/fstl-e-android/releases/download/v1.0.3/fstl-e-android-v1.0.3-arm64.apk)

Includes full OCCT STEP support for enhanced geometry loading.

> **Release validation:** A replacement build is being validated. See [Play readiness](docs/PLAY_READINESS.md) for the remaining signing, device and release checks.
> 
> APK upgrades require a compatible signing identity. Preserve app settings while checking signing compatibility.

### What's New in v1.0.3

- **Fixed critical crash** in Background Color Settings preset dropdown
- **Integrated OCCT libraries** for enhanced STEP file support
- Replaced problematic QComboBox with stable inline list widget on Android
- Improved stability and reliability across all Android devices

## Features

- Fast rendering of STL (binary and ASCII), 3MF, and STEP files
- Multiple draw modes: Shaded, Wireframe, Surface Angle, Meshlight
- Configurable lighting and shader preferences
- Touch gestures: pinch to zoom, drag to rotate
- Auto-reload on file changes
- Displays mesh information (triangle count, dimensions)
- Supports Android's scoped storage (content URIs)

**Note:** STEP file support uses OpenCASCADE (OCCT) libraries and supports most standard STEP geometry.

## Building

### Android (with OCCT)

The CI build pins Qt 6.10.0 and builds OCCT from source; OCCT binaries are not bundled in this repository. See [CI/CD and local build instructions](docs/CI_CD.md) for the SDK paths, full STEP dependency build and release checks.

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$QT_ROOT/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
  -DFSTL_OCCT_ROOT=/path/to/android/occt-install -DFSTL_REQUIRE_OCCT=ON ..
cmake --build .
```

### Desktop (with optional OCCT STEP support)

```bash
mkdir build-desktop && cd build-desktop
cmake .. -DENABLE_OCCT_STEP=ON   # assumes OpenCASCADE is installed
cmake --build .
```

When `ENABLE_OCCT_STEP=ON` and OpenCASCADE is found, the viewer will use the
OCCT kernel for STEP files and reject imports that OCCT cannot triangulate.
The reduced internal parser is used only in builds without OCCT.

## License

MIT License - see LICENSE file

### Third-party components

- STEP file support uses Open CASCADE Technology (OCCT). OCCT is free software
  licensed under the GNU Lesser General Public License (LGPL) version 2.1 with
  the Open CASCADE exception. See the OCCT licensing information for details.

## Credits

- Original fstl: [Matt Keeter](https://github.com/fstl-app/fstl)
- fstl-e enhancements: [William Daniau](https://github.com/wdaniau/fstl)
- Android port: Prj-m and contributors
