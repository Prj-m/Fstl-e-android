# Testing program and release gates

The first delivery target is Google Play internal testing. Production delivery remains disabled until the gates below have evidence. The current unsigned validation bundle and separate development APK do not establish production signing identity or Play upgrade compatibility.

## Gate 1: security and build review

Require passing desktop regression and Android bundle jobs on the exact reviewed commit. Protect main against direct/force pushes and deletion, require pull requests and the two build checks, and enforce the policy for administrators. The current GitHub connection cannot apply protection settings.

Before accepting external models, resolve the warm file-open failure observed in phone smoke testing; verify local/cloud provider permissions. Review hostile ZIP decompression limits, OCCT resource consumption and multi-object 3MF behavior. Maintain malformed-input sanitizer coverage and add meaningful fixtures for fixes. Review packaged dependency versions against published advisories and record applicable findings, resolutions and remaining risks.

Validate the final packaged manifest (release not debuggable, backup and cleartext disabled, reviewed permissions/components), every native library's 16 KB ELF alignment, generated APK ZIP alignment and Android 16/16 KB runtime behavior. Verify privacy and dependency/license notices against the actual package. Passing static checks alone cannot approve release.

## Gate 2: internal testing delivery

Verify Console account/track state, highest uploaded version code, upload certificate and app-signing certificate. Preserve existing keys. Increment the version code above all prior uploads. Build and sign with the confirmed upload identity using a protected, manually triggered delivery workflow with environment-scoped credentials and minimum track permissions. Do not give credentials to PR jobs.

Upload to internal testing, retain the commit, artifact SHA-256, version code, upload identity and Actions run ID. Check fresh installation and upgrades through Play; review the pre-launch report before inviting a wider group. No automatic production promotion.

## Gate 3: user testing

Invite actual CAD/3D-printing users with supported Android phones. Give them small generated/public fixtures and ask them to avoid confidential models. Include different Android versions, graphics hardware, page sizes and folding/tablet layouts. Use a private feedback form or issue intake; provide a contact route and explain what diagnostics are collected. Do not request personal models, account credentials or unrelated device logs.

Use these tasks:

- Install through Play, open the app and load valid STL, single-object 3MF and STEP samples from local and cloud storage.
- Open a second model while the app is running; restart and check permission/reload behavior.
- Rotate, zoom, change display settings, fold/unfold where available, background/resume and save a screenshot.
- Open malformed and budget-exceeding samples; verify a clear error without a crash or prolonged unresponsiveness.
- Compare expected geometry and triangle counts against trusted reference tools, especially multi-object 3MF and STEP.
- Upgrade from the previous testing version and confirm settings/data remain available.

For each report collect the app version, Android version, device model, task, expected/actual behavior and reproducible steps. Let testers attach a screenshot or a nonconfidential sample voluntarily. Record task completion, confusing interactions, import time and perceived usefulness; avoid invasive analytics merely to measure satisfaction.

Review feedback each testing round. A crash, data loss, wrong geometry, failed update or security issue blocks expansion. Fix reproducible failures, publish a new testing version, and ask affected testers to retest. Track issues to a resolution and keep testers informed through release notes prepared for review.

## Gate 4: production decision

Require no unresolved critical/high security findings, no known reproducible crashes/data loss/wrong-geometry issues in supported scenarios, passing exact-commit CI, completed runtime/upgrade checks and reviewed store declarations. Record the scope and limits of testing; do not promise universal security or compatibility.

Check the account-specific production access requirement in Console. Some newer personal developer accounts require a closed test with at least 12 opted-in testers continuously for 14 days; internal testing does not substitute for this. Reconfirm current policy before scheduling a release.

Promote only after reviewing evidence and user feedback. Prepare rollback/halt instructions for the testing/production track and a higher-version-code corrective release; Android installs generally cannot downgrade in place.

References: [Play testing tracks](https://support.google.com/googleplay/android-developer/answer/9845334), [personal-account testing requirements](https://support.google.com/googleplay/android-developer/answer/14151465), [pre-launch reports](https://support.google.com/googleplay/android-developer/answer/9842757).
