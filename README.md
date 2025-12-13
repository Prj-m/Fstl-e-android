# fstl-e for Android

<p align="center"><img src="screenshots/android_app_ui_20251125_191127.png" alt="UI screenshot (Android)" width="600"></p>


**Status: Beta** - Stable with all core features working. Ready for production use.

Android port of [fstl-e](https://github.com/wdaniau/fstl), a fast STL, 3MF, and STEP file viewer.

## Download

**Google Play (Recommended):**

[![Get it on Google Play](https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png)](https://play.google.com/store/apps/details?id=com.github.prjm.fstl_e)

**Direct APK Downloads:**

For users who prefer sideloading or don't have access to Google Play:

- **Lite APK** (v1.0.3, ~40MB): [Download](https://github.com/Prj-m/fstl-e-android/releases/download/v1.0.3/fstl-e-android-v1.0.3-lite.apk)
- **Full APK** (v1.0.3, ~40MB): [Download](https://github.com/Prj-m/fstl-e-android/releases/download/v1.0.3/fstl-e-android-v1.0.3-full.apk)

Both APKs are functionally identical and include full OCCT STEP support.

> **Note:** Google Play version includes all features and receives automatic updates.
> 
> **Important:** Users upgrading from v1.0.2 must uninstall the old version first due to signing certificate change.

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

Requires Qt 6.5+ for Android (tested with Qt 6.10). OCCT libraries are bundled in the `android/libs` directory.

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

### Third-party components

- STEP file support uses Open CASCADE Technology (OCCT). OCCT is free software
  licensed under the GNU Lesser General Public License (LGPL) version 2.1 with
  the Open CASCADE exception. See the OCCT licensing information for details.

## Credits

- Original fstl: [Matt Keeter](https://github.com/fstl-app/fstl)
- fstl-e enhancements: [William Daniau](https://github.com/wdaniau/fstl)
- Android port: Prj-m and contributors
