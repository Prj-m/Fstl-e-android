# fstl-e for Android

<p align="center"><img src="screenshots/android_app_ui_20251125_191127.png" alt="fstl-e Android viewer" width="600"></p>

Android port of [fstl-e](https://github.com/wdaniau/fstl), a viewer for STL, 3MF and STEP models. The current candidate is a testing prerelease; production readiness remains under review.

## Testing

Closed test on Google Play, build 1.0.4-rc.5. Join the tester group, then opt in: [play.google.com/apps/testing/com.github.prjm.fstl_e](https://play.google.com/apps/testing/com.github.prjm.fstl_e)

Android 9+, arm64. Report problems with the [bug report template](https://github.com/Prj-m/fstl-e-android/issues/new?template=bug_report.md).

## Building

Qt 6.11.3, JDK 17, SDK 36, NDK 27.2.12479018, OCCT 7.9.3. See [docs/CI_CD.md](docs/CI_CD.md).

## License and credits

Application source is covered by the [MIT license](LICENSE). STEP support uses OCCT under LGPL 2.1 with the Open CASCADE exception. Qt and other dependencies have their own licenses.

- Original fstl: [Matt Keeter](https://github.com/fstl-app/fstl).
- fstl-e enhancements: [William Daniau](https://github.com/wdaniau/fstl).
- Android port: Prj-m and contributors.
