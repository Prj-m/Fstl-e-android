# fstl-e 1.0.4-rc.1 — testing prerelease

Hardens model imports and Android file handling, with bounded 3MF decompression, malformed-input checks, warm file-open support and screenshot saving through Android's document picker. Removes unused Qt dependencies and adds pinned CI plus manifest/native-memory alignment checks. STEP geometry containing nonfinite float coordinates is rejected.

This is a testing prerelease. Android development APKs use the separate `com.github.prjm.fstl_e.dev` identity and do not replace the Play app. Unsigned AABs are validation artifacts, not installable releases or Play-ready uploads. No production promotion is included.

Known limits: Android-version/device coverage and native 16 KB runtime testing are incomplete; cloud providers and OCCT resource stress need further checks. Current 3MF support is a single untransformed mesh. Dependency attribution and Play distribution checks remain open.

Feedback: https://github.com/Prj-m/fstl-e-android/issues . Report app version, device/Android version, steps and expected versus actual behavior. Use synthetic or nonconfidential models. Do not attach signing keys, passwords, private account details, confidential models or unrelated device logs.
