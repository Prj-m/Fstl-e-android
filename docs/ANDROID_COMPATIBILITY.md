# Android compatibility evidence

Test the same arm64 native code and dependency versions intended for Play. A
separate `.dev` APK is useful for local tests; it does not establish Play signing,
installation or update compatibility. Record commit, artifact SHA-256, device,
API level, ABI and `adb shell getconf PAGESIZE` for every run.

| Environment | Purpose | Evidence so far |
| --- | --- | --- |
| Android 9 / API 28, arm64 | Oldest supported API and legacy storage behavior | Pending |
| Android 13 / API 33, arm64 | Storage permission transition | Pending |
| Android 14 / API 34, Fold4, 4 KB | Native imports, document provider and foldable layout | Warm STL/3MF/STEP imports, cube rendering, Downloads-provider 3MF and native-picker PNG save passed on 54d54a2; folding/rotation and cloud-provider checks pending |
| Android 15 / API 35, arm64 | Newer platform behavior | Pending |
| Android 15/16, arm64, 16 KB | Native loading, memory protection and geometry libraries | Pending; static LOAD and rounded RELRO checks do not replace this run |
| Android 16 / API 36, arm64 | Target-SDK behavior, layout and document-provider handling | Pending |

For each environment check cold and warm imports, known geometry/triangle counts,
rotate/zoom, background/resume, document-picker cancellation, screenshot save and
reopen, and malformed-file rejection followed by a valid import. Test STEP through
a content URI and verify its temporary copy is removed after success/failure.
Record import time and unexpected memory growth; OCCT's internal allocations and
meshing time are not hard-bounded by the source-file or output-triangle limits.

For a 16 KB test, confirm the page size is 16384 and record whether page-size
compatibility mode is enabled. A run using compatibility mode is not evidence of
native 16 KB compatibility. Collect only app-specific diagnostics and synthetic
samples; do not collect unrelated logs or private models.

The current workstation is x86_64 and the app is arm64-only. ARM images cannot
use x86 VM acceleration. Do not count an x86 build or a different dependency set
as validation of the arm64 release. Use a supported ARM emulator host or physical
arm64 devices where local emulator support is impractical. Paid testers must
report actual device/API coverage and completed tasks; tester count alone is not
a compatibility matrix.

References: [Android page sizes](https://developer.android.com/guide/practices/page-sizes),
[emulator acceleration](https://developer.android.com/studio/run/emulator-acceleration),
[AOSP RELRO protection implementation](https://android.googlesource.com/platform/bionic/+/main/linker/linker_phdr.cpp).
