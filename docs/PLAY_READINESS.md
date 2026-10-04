# Google Play release readiness

Status on 2026-10-04: **not ready for production**. Local checks are evidence of specific behavior, not a security certification. No Play Console state has been verified.

## Evidence and release blockers

| Gate | Evidence | Required before production |
| --- | --- | --- |
| Existing installation | Samsung SM-F936U, Android 14/API 34, arm64, 4 KB pages; v1.0.3/code 19 launched successfully | Test the new release candidate, including Play installation and upgrades |
| Native compatibility | Installed APK fails 16 KB ELF alignment at `libTKBO.so` | Rebuild all native dependencies; verify AAB, generated APK ZIP alignment and 16 KB runtime |
| Signing | Installed APK is debuggable; extracted v2 certificate subject is Android Debug | Verify signatures with apksigner; compare Play upload and app-signing fingerprints; preserve the existing keys |
| API target | Installed APK targets 35; source now targets 36 | Confirm packaged manifest and Android 16 behavior |
| Parser safety | 25 host sanitizer checks pass for malformed STL/3MF; arithmetic/indexing and STEP temporary-file fixes prepared | Bound file/decompression/mesh resources and test malformed/large inputs on device; review fallback STEP and multi-object 3MF correctness |
| CI | 16 local pipeline unit tests and host viewer compile pass | Hosted desktop and Android workflows must pass on the exact reviewed commit |
| Package hardening | Release Gradle configuration disables debugging; source disables backup and cleartext traffic | Confirm the merged release manifest, permissions, component exports and packaged SDKs |
| Privacy/licensing | Source has no INTERNET permission or identified analytics/ads integration | Verify final package; publish accurate privacy policy, Data safety and Qt/OCCT notices |
| Play delivery | Console account, track state, highest code and credentials unknown | Establish signing identity, increment version code, use internal testing and examine the pre-launch report |

Installed APK extracted public certificate SHA-256:

`e79cd0680f942ce50dfd44041aae51fd4d29a8a3b65bcbda22e0208b5e2f5c35`

Extraction does not verify an APK signature and does not establish which identity Play uses. Never upload debug-signed `.dev` APKs to Play. The CI phone package uses a separate application ID; testing it cannot establish upgrade compatibility for the release package.

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
