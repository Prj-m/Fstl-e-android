# Google Play release readiness

Status on 2026-10-04: **not ready for production**. Local checks are evidence of specific behavior, not a security certification. No Play Console state has been verified.

## Evidence and release blockers

| Gate | Evidence | Required before production |
| --- | --- | --- |
| Existing installation | Samsung SM-F936U, Android 14/API 34, arm64, 4 KB pages; v1.0.3/code 19 launched successfully | Test the new release candidate, including Play installation and upgrades |
| Native compatibility | Installed APK fails 16 KB ELF alignment at `libTKBO.so` | Rebuild all native dependencies; verify AAB, generated APK ZIP alignment and 16 KB runtime |
| Signing | Installed APK is debuggable; extracted v2 certificate subject is Android Debug | Installed APK signature verified with apksigner; compare Play upload and app-signing fingerprints; preserve the existing keys |
| API target | Installed APK targets 35; source now targets 36 | Confirm packaged manifest and Android 16 behavior |
| Parser safety | 30 host import/URI sanitizer checks pass for malformed STL/3MF; arithmetic/indexing and STEP temporary-file fixes prepared | Input, declared XML and output mesh budgets added; test device memory/CPU behavior and deceptive ZIP metadata, and review multi-object 3MF correctness |
| Branch protection | GitHub reports `main` unprotected and no rulesets | Require PRs and desktop/Android checks; block force pushes and deletion; current connector cannot change administration settings |
| CI | 26 local pipeline unit tests and host viewer compile pass | Hosted desktop and Android workflows must pass on the exact reviewed commit |
| Dependency updates | Qt pin updated from 6.10.0 to 6.11.3 after checking official advisories | Verify packaged modules/SBOM against relevant advisories and third-party vulnerabilities |
| Package hardening | Release Gradle configuration disables debugging; source disables backup and cleartext traffic | Confirm the merged release manifest, permissions, component exports and packaged SDKs |
| Privacy/licensing | Source has no INTERNET permission or identified analytics/ads integration | Verify final package; publish accurate privacy policy, Data safety and Qt/OCCT notices |
| Play delivery | Console account, track state, highest code and credentials unknown | Establish signing identity, increment version code, use internal testing and examine the pre-launch report |

Installed APK extracted public certificate SHA-256:

`e79cd0680f942ce50dfd44041aae51fd4d29a8a3b65bcbda22e0208b5e2f5c35`

The installed APK signature was subsequently verified with apksigner. This does not establish which identity Play uses. Never upload debug-signed `.dev` APKs to Play. The CI phone package uses a separate application ID; testing it cannot establish upgrade compatibility for the release package.

## Production acceptance

1. Preserve and verify the upload/app-signing identities against Play Console. Record the highest version code and account testing requirements. Keep private keys and passwords outside source/chat/logs.
2. Merge only after CI and the full Android dependency build pass. Pin dependencies/actions, retain artifact hashes and run IDs, and scan dependency versions for relevant published vulnerabilities.
3. Build a non-debuggable signed release AAB targeting API 36 with a version code above every previously uploaded code. Verify the final manifest, signatures, native libraries and generated APK alignment. Never infer these from source configuration alone.
4. Exercise startup, second file-open intents, provider URI permissions, local/cloud STL/3MF/STEP imports, malformed/large files, settings, gestures, fold/unfold/rotation, background/restart, screenshots and storage. Include Android 16 and 16 KB runtime tests.
5. Upload to internal testing with the confirmed upload identity. Test fresh installation and updates through Play, inspect pre-launch crashes/warnings, and complete any account-specific closed-testing requirement.
6. Verify store declarations, privacy URL, content rating, third-party license obligations and contact information. Approve production promotion only after this evidence is recorded.

Signed delivery should be a separate protected workflow using environment secrets, approved main-branch commits and minimum Play permissions. Public PR builds receive no signing or Play credentials. Do not enable automatic production promotion while these gates remain open.

## Official references

- [Target API requirements](https://developer.android.com/google/play/requirements/target-sdk)
- [16 KB compatibility](https://developer.android.com/guide/practices/page-sizes)
- [Play App Signing](https://support.google.com/googleplay/android-developer/answer/9842756)
- [Pre-launch reports](https://support.google.com/googleplay/android-developer/answer/9842757)
- [Personal-account testing requirements](https://support.google.com/googleplay/android-developer/answer/14151465)

Dependency advisory source: [Qt known vulnerabilities](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products). The version update does not prove every advisory is resolved or relevant; reachability and bundled third-party dependencies still require review.

The Android workflow uses checksummed bundletool 1.18.3 to validate the AAB structure and dump the actual packaged manifest. Automated checks reject a debug release, wrong package, target API below 36, enabled backup/cleartext, new unreviewed permissions and additional exported components. Set `FSTL_PLAY_HIGHEST_VERSION_CODE` when running `bash scripts/check_android_release.sh <release.aab>` to enforce the Console version floor. This validator does not authenticate the upload signing identity or authorize production.

Import limits now reject source files over 128 MiB, declared/extracted 3MF model XML over 32 MiB, over 1,000,000 3MF coordinates and over 1,000,000 output triangles. STEP staging counts bytes as it copies. STL file stability polling is bounded. These limits do not bound OCCT internal allocation/meshing time or guarantee a hard decompression memory limit when ZIP size metadata lies. The full-OCCT release rejects failed STEP imports instead of invoking the reduced fallback that can invent geometry from points. Import-thread exceptions are converted to an error signal; process termination and library aborts remain outside that mechanism.

Branch policy should enforce checks for administrators as well. Require an independent approval when a second maintainer is available; the owner cannot approve their own PR. Requiring an unavailable reviewer would lock a solo-maintainer workflow. No protection setting has been changed.

Local candidate validation: unsigned full-STEP AAB built with Qt 6.11.3, API 36 and NDK r27c; Release lint passed and all 43 native libraries passed ELF alignment. The actual packaged manifest passed policy checks. AndroidX ProfileInstallReceiver is allowed only with its exact class name and the privileged `android.permission.DUMP` guard; unguarded variants are rejected. The `.dev` APK passed Debug lint, apksigner, 16 KB ZIP alignment and package ID checks. The separate development APK was installed on an authorized Samsung SM-F936U (Android 14, 4 KB pages). Cold-start loader logs confirm a one-triangle 3MF and a 12-triangle OCCT STEP cube; a malformed STL did not terminate the process. These checks do not establish rendering correctness. Warm VIEW intents did not reach the loader in the smoke test and remain a release blocker. The geometry shader emitted a version warning and the existing fallback path needs visual verification. Device testing was paused at the owner’s request; Android 16, 16 KB runtime, provider URI permissions and final hosted results remain pending.

The staged tester tasks, issue intake and release gates are in [TESTING_PROGRAM.md](TESTING_PROGRAM.md).
