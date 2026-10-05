# fstl-e for Android

<p align="center"><img src="screenshots/android_app_ui_20251125_191127.png" alt="fstl-e Android viewer" width="600"></p>

Android port of [fstl-e](https://github.com/wdaniau/fstl), a viewer for STL, 3MF and STEP models. The current candidate is a testing prerelease; production readiness remains under review.

## Download and testing

The [1.0.4-rc.1 testing release](https://github.com/Prj-m/fstl-e-android/releases/tag/v1.0.4-rc.1) includes an arm64 development APK and its SHA-256 checksum. Verify the checksum before installation. This debug-signed APK uses `com.github.prjm.fstl_e.dev`, installs alongside the Play app, and is intended for evaluation. Native dependencies still contain generic build-path strings.

The [Google Play listing](https://play.google.com/store/apps/details?id=com.github.prjm.fstl_e) may be restricted to a testing track; public availability must be confirmed in Play Console. The development APK is not a Play upload artifact.

Other Android versions and native 16 KB runtime compatibility still require testing. Report reproducible issues with nonconfidential models through [GitHub Issues](https://github.com/Prj-m/fstl-e-android/issues).

## Features and import limits

- STL import in binary and ASCII formats, bounded by source and mesh limits.
- 3MF import for a single untransformed mesh; unsupported assemblies and transforms are rejected.
- STEP import and triangulation with Open CASCADE Technology (OCCT) in the Android build. OCCT failures are reported rather than replaced with guessed geometry.
- Shaded, wireframe, surface-angle and meshlight rendering, with configurable lighting.
- Touch rotation and zoom, mesh information, and Android document-provider file selection.
- Screenshot saving through Android's document picker.

Source/model sizes and output mesh counts are bounded. OCCT internal allocations and meshing time remain a resource-review gap. File-provider behavior, background/resume and graphics compatibility need coverage across the supported device matrix.

## Building

### Android with OCCT

CI pins Qt 6.11.3, JDK 17, SDK API 36, NDK 27.2.12479018 and OCCT 7.9.3. Dependencies are built or installed separately; their binaries are not checked into Git. Follow [CI/CD and local build instructions](docs/CI_CD.md) to set the SDK and dependency paths, build OCCT, generate the unsigned AAB and run release checks.

For phone testing, the documented packaging script generates the separate development APK. Signing and Play delivery require the confirmed upload identity and a version code higher than every existing Console upload or draft.

### Desktop

With Qt 6.11.3, CMake, Ninja and zlib installed:

```bash
cmake -S . -B build-desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_OCCT_STEP=OFF
cmake --build build-desktop
```

To use OCCT STEP support, install Open CASCADE and configure with `-DENABLE_OCCT_STEP=ON`. Verify that CMake finds OCCT; the reduced internal STEP parser is used in builds without it.

## Development and release checks

CI scans source history for credentials and device/workstation metadata, runs shell checks and pipeline tests, and builds the desktop viewer with parser sanitizer regressions. Android validation also checks packaging, lint, the actual bundle manifest and native LOAD/RELRO alignment. These checks do not establish compatibility on every device.

See [privacy information](PRIVACY.md) for data handling. Keep signing keys, account details, device serials and private models out of public source, issues and logs.

## License and credits

Application source is covered by the [MIT license](LICENSE). STEP support uses OCCT under LGPL 2.1 with the Open CASCADE exception. Qt and other dependencies have their own licenses; dependency notices and source/relink distribution review remain release gates.

- Original fstl: [Matt Keeter](https://github.com/fstl-app/fstl).
- fstl-e enhancements: [William Daniau](https://github.com/wdaniau/fstl).
- Android port: Prj-m and contributors.
