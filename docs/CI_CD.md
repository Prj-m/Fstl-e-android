# CI/CD

## Workflows

- **CI:** privacy scan, shell and Python checks, parser sanitizer tests, desktop build.
- **Android validation:** OCCT build, Android build, lint, manifest and 16 KB alignment checks, development APK.

Both run on pull requests. Artifacts are kept for 14 days.

## Local Android build

Requires JDK 17, Ninja, and the Qt, SDK, NDK and OCCT versions pinned in [android-bundle.yml](../.github/workflows/android-bundle.yml).

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

The AAB is unsigned. Signing keys are kept outside the repository.

## Install on a device

```bash
bash scripts/package_android_phone_apk.sh
export ANDROID_SERIAL="YOUR_DEVICE_SERIAL"
bash scripts/deploy_android_phone.sh build/android-release/phone-artifacts/fstl-e-arm64-dev.apk
```

The development APK uses the package `com.github.prjm.fstl_e.dev` and installs alongside the Play app.
