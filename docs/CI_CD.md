# CI/CD

The workflows validate source and build an unsigned AAB plus a separate development APK. Play delivery is not automated.

## CI checks

- **CI:** source-history privacy scan, shell/Python checks, parser sanitizer tests and a desktop build.
- **Android validation:** OCCT build, Android compilation, lint, manifest/native-alignment checks and development APK signature/ZIP-alignment checks.

Pull requests run both workflows. Failed builds do not publish artifacts; successful artifacts are retained for 14 days. Static checks do not establish compatibility on every Android device.

## Local Android build

Install JDK 17, Ninja and the Qt/SDK/NDK versions pinned in the [Android workflow](../.github/workflows/android-bundle.yml). Check out the OCCT commit specified there. Set `JAVA_HOME` and add the JDK and SDK tools to `PATH`.

From the repository root:

```bash
export ANDROID_SDK_ROOT="/path/to/Android/Sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
export QT_ANDROID_ROOT="/path/to/Qt/6.11.3/android_arm64_v8a"
export QT_HOST_ROOT="/path/to/Qt/6.11.3/gcc_64"
export FSTL_OCCT_SOURCE="/path/to/occt-source"
export FSTL_OCCT_ROOT="/path/to/occt-install"
bash scripts/build_occt_android.sh
bash scripts/build_android_full_release_aab.sh
bash scripts/check_android_release.sh build/android-release/artifacts/fstl-e-arm64-release.aab
```

The resulting AAB is unsigned and cannot be installed directly or submitted to Play. Keep signing material outside tracked source; see [Play readiness](PLAY_READINESS.md) before preparing a signed upload.

## Test on a phone

Connect an authorized USB-debugging device, then run:

```bash
bash scripts/package_android_phone_apk.sh
export ANDROID_SERIAL="YOUR_DEVICE_SERIAL"
bash scripts/deploy_android_phone.sh build/android-release/phone-artifacts/fstl-e-arm64-dev.apk
```

The APK uses `com.github.prjm.fstl_e.dev` and a test signing key. It installs alongside the Play app; it is not a Play release. CI test keys can change between runs, so a later APK may not upgrade an earlier test installation.

Use the [compatibility checklist](ANDROID_COMPATIBILITY.md) for device testing and [contribution guidance](../CONTRIBUTING.md) for source/privacy checks. Keep device serials and workstation records private.
