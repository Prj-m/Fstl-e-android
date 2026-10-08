# Third-party components

`NOTICE.txt` is shown in the app under About → Licenses.

## Versions

- Qt 6.11.3 (LGPL 3, dynamically linked): qtbase `5a1194b2d368e2d72c230205c464b04a2f23eccd`, qtsvg `cb69025ce26ced80be7a96b6293fa5333fd42000`. Source: https://download.qt.io/archive/qt/6.11/6.11.3/single/
- Open CASCADE 7.9.3 (LGPL 2.1 with exception, unmodified): `a016080bf6738d6aeae020badee4e888ad1540a5`. Source: https://github.com/Open-Cascade-SAS/OCCT/tree/V7_9_3
- Android NDK r27c (`27.2.12479018`) libc++ runtime.
- Java dependencies: see `java-runtime-inventory.json`.

## Rebuilding with modified libraries

Qt, OCCT and libc++ are shared libraries and can be replaced. Build your versions, point `QT_ANDROID_ROOT`, `QT_HOST_ROOT` and `FSTL_OCCT_ROOT` at them, and run `scripts/build_android_full_release_aab.sh` with `FSTL_SIGN_WITH_KEYSTORE=0`. Keep the packaged ABIs (`FSTL_ANDROID_ABIS`) and 16 KB alignment for 64-bit libraries, and verify with `scripts/check_android_release.sh`. `scripts/package_android_phone_apk.sh` builds an installable APK signed with your own debug key; no project key is needed.
