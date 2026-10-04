# Google Play release readiness

The current release candidate is intended for testing, not production.

## Validated

Pinned desktop/Android CI, parser sanitizer regressions, release lint, packaged manifest policy, native LOAD/RELRO checks and development APK signature/ZIP alignment passed the prior candidate. Android 14 physical-device tests covered warm STL/3MF/STEP imports, invalid-file recovery, STEP through the Downloads provider, temporary-file cleanup, screenshot saving and background/resume. Every subsequent code change requires fresh checks.

The viewer bounds source/model sizes and output mesh counts. It rejects unsupported 3MF assemblies/transforms and malformed ZIP metadata/inflation rather than rendering guessed geometry. OCCT internal allocations and meshing time remain a resource-review gap.

## Before Play delivery

- Confirm the upload identity is active in Console. Keep private keys and passwords local and out of source, chat, CI logs and tester submissions.
- Verify every uploaded/draft version code before using the candidate's code 27; increase it if necessary.
- Validate the final signed bundle and actual manifest. Never submit the separate `.dev` APK to Play.
- Complete other supported Android-version, cloud-provider and native 16 KB runtime tests. Static checks do not establish runtime compatibility.
- Complete dependency/embedded third-party notices and source/relink distribution review, plus privacy, Data safety and store declarations.
- Inspect Play pre-launch results and verify installation/upgrades through Play before widening access.

## Closed testing and production

Use the reviewed updated Play build for closed testing. Preserve existing tester access, verify opt-in/feedback routes and give testers synthetic samples and explicit tasks. Record actual device coverage and feedback; paid tester counts do not establish compatibility or guarantee production access.

Require PRs and passing CI before merging release changes. Production promotion requires completed account-specific testing requirements and resolved release-blocking findings.

See [Android compatibility](ANDROID_COMPATIBILITY.md) and [testing program](TESTING_PROGRAM.md).
