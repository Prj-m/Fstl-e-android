# fstl-e for Android

<p align="center"><img src="screenshots/android_app_ui_20251125_191127.png" alt="fstl-e Android viewer" width="600"></p>

Android port of [fstl-e](https://github.com/wdaniau/fstl), a viewer for STL, 3MF and STEP models. The current candidate is a testing prerelease; production readiness remains under review.

## Testing on Google Play

fstl-e is in closed testing on Google Play. Current test build: **1.0.4-rc.5** (version code 31).

1. Join the tester group or list you were invited to, using the same Google account as your phone.
2. Opt in: **[play.google.com/apps/testing/com.github.prjm.fstl_e](https://play.google.com/apps/testing/com.github.prjm.fstl_e)**
3. Install or update **fstl-e** from Google Play and keep it installed for the whole test period.

Requires an arm64 phone or tablet running Android 9 or newer. The app works offline and needs no account.

**Please test:** open STL, 3MF and STEP files from local storage and from cloud storage (for example Google Drive); rotate and zoom; open a second model from a file manager while the app is running; rotate the device, background and resume; save a screenshot with the camera button.

### Reporting a problem

Open a [bug report](https://github.com/Prj-m/fstl-e-android/issues/new?template=bug_report.md). Include the app version (shown under the info button), phone model, Android version, the steps you took, and what you expected versus what happened. Attach a screenshot when helpful. Only share models that are not confidential, and leave out personal data.

Older debug APKs on the Releases page predate fixes in the Play build. Use Google Play for testing.

## Source and builds

Releases are built from the default branch. The release build uses Qt 6.11.3, JDK 17, SDK API 36, NDK 27.2.12479018 and OCCT 7.9.3. Dependencies are built or installed separately; their binaries are not tracked in Git. See [CI/CD and local builds](docs/CI_CD.md) for build and validation steps.

Compatibility across Android versions and native 16 KB page-size devices is being checked during closed testing.

## License and credits

Application source is covered by the [MIT license](LICENSE). STEP support uses OCCT under LGPL 2.1 with the Open CASCADE exception. Qt and other dependencies have their own licenses.

- Original fstl: [Matt Keeter](https://github.com/fstl-app/fstl).
- fstl-e enhancements: [William Daniau](https://github.com/wdaniau/fstl).
- Android port: Prj-m and contributors.
