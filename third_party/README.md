# Dependency notices and replacement builds

`NOTICE.txt` is included in the application at About → Licenses. It preserves the application MIT notice, Qt and embedded-component attributions, OCCT LGPL 2.1 and exception, Java/Kotlin artifact license metadata, and NDK LLVM/libc++ notices. The Qt inventory is a conservative dependency closure from vendor SPDX for the actual packaged modules/plugins; it can include generation/build metadata. It is not a claim that every item is a separately shipped binary.

## Source versions

- Viewer: the exact candidate commit recorded in the release validation report; obtain its GitHub source archive or checkout.
- Qt 6.11.3: Qt Base commit `5a1194b2d368e2d72c230205c464b04a2f23eccd`, Qt SVG commit `cb69025ce26ced80be7a96b6293fa5333fd42000`, as recorded by the installed kit's SPDX. Official sources: https://code.qt.io/qt/qtbase.git/ and https://code.qt.io/qt/qtsvg.git/ ; archive download: https://download.qt.io/archive/qt/6.11/6.11.3/single/ . Qt is dynamically linked under LGPL 3, with full LGPL/GPL texts in the notices.
- Open CASCADE 7.9.3: commit `a016080bf6738d6aeae020badee4e888ad1540a5`. Sources: https://github.com/Open-Cascade-SAS/OCCT/tree/V7_9_3 . Build flags: `scripts/build_occt_android.sh`. No local OCCT source modifications are used.
- Android NDK r27c (`27.2.12479018`): distributed LLVM/libc++ shared runtime and notices. NDK source/build documentation: https://android.googlesource.com/platform/ndk/ and https://android.googlesource.com/toolchain/llvm_android/ .
- Java dependencies: exact resolved Maven coordinates, artifact SHA256 and declared license metadata are in `java-runtime-inventory.json`. Maven source artifacts can be obtained for those coordinates from Google Maven or Maven Central. The Guava listenablefuture license is inherited from its cached parent POM.

## Rebuild and replace

Install JDK 17, Android SDK platform/build-tools 36, NDK r27c, and Qt 6.11.3 Android arm64 plus matching desktop host tools. Set `QT_ANDROID_ROOT`, `QT_HOST_ROOT`, `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, and `FSTL_OCCT_ROOT` to your own installations; no signing secret is needed to build an unsigned AAB. Build OCCT using `scripts/build_occt_android.sh`, then run `scripts/build_android_full_release_aab.sh` with `FSTL_SIGN_WITH_KEYSTORE=0`.

The native viewer, Qt modules/plugins, OCCT and libc++ are shared libraries. To use modified compatible dependencies, build/install them to your own prefixes and supply those prefixes to the build. Preserve the arm64 ABI and 16 KB LOAD/RELRO alignment. Validate the resulting bundle with `scripts/check_android_release.sh`.

Run `scripts/package_android_phone_apk.sh` to produce a separately debug-signed `.dev` APK using your own debug signing identity. It can be installed alongside the Play application; no developer account, original private signing key or service login is needed. A modified production-package APK must use your own signature and may require uninstalling the Google-signed package first. Google Play's signing key is not needed to inspect, modify, rebuild, relink or run the separate development package.

Source-support archives should accompany the final binary distribution. Candidate source kits are prepared separately from the APK, and their exact hashes belong in the release validation record. Do not substitute a notice inventory for verifying the shipped binary and source correspondence.
