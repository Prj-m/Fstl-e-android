# Google Play release readiness

Status on 2026-10-04: **not ready for Play upload or production**. Specific checks pass; signing, runtime and Console gates remain open.

## Verified evidence

- Authenticated Console: package `com.github.prjm.fstl_e`, Play App Signing enabled, closed track REL25, published code 26/version 0.1.0-alpha.1. Four testers opted in; twelve continuous opt-ins for fourteen days are required before applying for production access. Verify all bundles and drafts before choosing the next code.
- Hosted desktop and Android CI passed commit `9de5216f04b052a466beabcbfa5d8714c724734b`. Subsequent bounded-ZIP and screenshot changes require their own hosted checks.
- Local unsigned full-STEP arm64 AAB targets API 36. Release lint, actual packaged manifest policy and sixteen-kilobyte ELF alignment passed for all 43 native libraries. This artifact still uses code 19 and cannot be uploaded over the existing release.
- Warm file-open intent handling passed on a Samsung SM-F936U running Android 14 with four-kilobyte pages. Successive STL/3MF/STEP imports and recovery after invalid STL passed in one process. Provider content-URI permissions and visual rendering still require validation.
- Thirty initial host sanitizer checks and 26 pipeline tests passed. Expanded archive tests now cover incorrect decompressed sizes, CRC, malformed offsets/lengths, duplicate model parts and unsupported 3MF assemblies.
- Main has an active ruleset preventing deletion and force pushes. Required PRs and required status checks are still missing.
- Testers Community dashboard confirms one paid Starter credit, zero apps submitted and zero tests running. Activation awaits the updated build and access configuration. No extra purchase is needed.

## Parser and storage hardening

Source inputs are limited to 128 MiB; 3MF model XML to 32 MiB; coordinate and output triangle counts to one million. STL count arithmetic, index ranges, finite coordinates and truncated records are checked. STEP staging counts bytes and uses a temporary-file lifetime guard. Full-OCCT imports reject failure rather than inventing fallback geometry.

The new ZIP reader limits actual inflate output, checks declared size and CRC, validates offsets and rejects ambiguous model parts. It supports ordinary single-disk stored/deflated ZIPs and rejects encryption and ZIP64. It does not extract archive paths to disk. The viewer currently accepts a single untransformed mesh; assemblies, component references and transforms are rejected to avoid displaying incorrect geometry. DTDs are rejected.

Android screenshot saving now uses the native document picker, preserves its content URI and writes PNG explicitly. Saving and canceling still need real-device validation. OCCT internal allocation and meshing time are not hard bounded; device stress testing remains necessary. These changes are not a security certification.

## Required before Play upload

1. Locate the upload keystore and verify its public fingerprint against Console. Keep key contents and passwords outside source, chat and logs. Upload certificate SHA-256: `b70b93e6ea12ade0f9dc5822e079dd024266819652a1f04c61823e173444bd61`.
2. Check all uploaded/draft version codes and increment the packaged code above them. Never upload the separate debug development APK.
3. Pass hosted CI on the final commit and validate the signed release AAB, actual manifest, permission/component policy, signature and native/ZIP alignment. Set `FSTL_PLAY_HIGHEST_VERSION_CODE` for the release validator.
4. Complete visual rendering, content-provider imports, screenshots, settings, rotation/folding and lifecycle tests. Include Android 15/16 and a sixteen-kilobyte runtime, plus different manufacturers and screen sizes.
5. Review dependency advisories and licensing, app declarations, privacy/Data safety and store information. Upload to internal testing only after these gates pass; inspect the Play pre-launch report and validate fresh install and upgrade through Play.

## Required before production

Use the updated reviewed release for paid closed testing. Preserve existing tester access, confirm the provider's supported task scope and configure a verified opt-in link and feedback route. Recruit real arm64 users across devices/Android versions, collect reproducible feedback, fix defects and retest. Complete Console's continuous testing requirement and provide truthful production-access answers based on the actual test.

Require PRs and the desktop/Android status checks before merging. Do not require an unavailable independent approver in a solo-maintainer repository. No production promotion has occurred.

## Official references

- [Target API requirements](https://developer.android.com/google/play/requirements/target-sdk)
- [16 KB compatibility](https://developer.android.com/guide/practices/page-sizes)
- [Play App Signing](https://support.google.com/googleplay/android-developer/answer/9842756)
- [Pre-launch reports](https://support.google.com/googleplay/android-developer/answer/9842757)
- [Personal-account testing requirements](https://support.google.com/googleplay/android-developer/answer/14151465)
- [Qt advisories](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products)

The staged tester tasks and release gates are in [TESTING_PROGRAM.md](TESTING_PROGRAM.md). Public certificate downloads and local audit details are retained outside the source tree; private credentials are not included.
