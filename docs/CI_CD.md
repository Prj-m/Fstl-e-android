# CI/CD for fstl-e Android

The validation workflows verify changes and produce an **unsigned** Android bundle plus a separate development APK. Signing and Google Play delivery require a confirmed upload identity and completed release checks.

## What runs

| Trigger | Workflow | Result |
| --- | --- | --- |
| Pull request, push to main, or manual run | CI | Secret/metadata source-history scan, shell checks, release-script/bundle tests, sanitizer parser checks, and a desktop compile |
| Pull request or Actions → Android validation bundle → Run workflow | Android validation bundle | The CI checks, a full OCCT STEP dependency build, Release Android compile, Gradle lint, native 16 KB alignment verification, an unsigned AAB with SHA-256 checksum, and an optional separate debug-signed `.dev` APK |

The Android build uses Qt 6.11.3, Java 17, SDK API 36, NDK 27.2.12479018, arm64-v8a and OCCT 7.9.3 at commit `a016080bf6738d6aeae020badee4e888ad1540a5`. OCCT is built with shared libraries and flexible page sizes; Qt and OCCT downloads/builds are cached. GitHub action references are pinned to commits. No application signing key or Play service account is required by either workflow. Tokens have read-only repository permissions.

Qt documents [NDK r27c and command-line Android builds](https://doc.qt.io/qt-6.11/android-building-projects-from-commandline.html). The API 36 migration also changes platform behavior; test layout, system bars and storage on Android 16 before approving a release.

The first Android run can take considerably longer while compiling OCCT. Android runs are queued per branch so an in-progress dependency build can finish and save its cache. Later runs reuse the cache. A workflow that fails does not upload its artifact. Packaging uses a fresh directory so an old successful AAB cannot hide a failed build. The native verifier checks ELF load segments, rounded RELRO protection, the ABI and required app/STEP libraries. Development APK packaging separately verifies ZIP alignment and signatures. Static checks do not establish Android runtime compatibility.

## Put the pipeline into service

1. Review and merge the workflow/build changes into the actual GitHub repository. A new manually dispatched workflow normally becomes available in Actions after it is on the default branch.
2. Confirm GitHub Actions is enabled. Run **CI** and resolve any hosted-runner build/lint differences before treating it as a merge gate.
3. Run **Android validation bundle**, select the reviewed branch and wait for all jobs. Download the `fstl-e-unsigned-arm64-<commit>` artifact from that run.
4. Verify the artifact checksum. The AAB is unsigned: it cannot be uploaded to Play or installed directly. For phone testing, generate APKs from it with bundletool using a local test key, or produce a signed local build using the confirmed production/upload identity. APKs signed with a different key cannot upgrade an existing installation; use a spare test device/profile rather than uninstalling an app with settings you want to keep.
5. Once the hosted workflows have passed, require pull requests and both **Build and regression checks**, **Repository privacy scan** and **Unsigned arm64 bundle with full STEP support** in the repository rules. Confirm the job names in the latest successful run before configuring them. Keep changes on branches and merge after checks pass.

The workflow also validates the actual AAB manifest with checksummed bundletool before artifact upload. `FSTL_PLAY_HIGHEST_VERSION_CODE` can enforce the Play version floor during local validation.

No branch rules, signing keys, Play Console state or GitHub releases are changed by these workflows. They build and retain validation artifacts for 14 days.

## Local Android build

Install the same Qt Android and host kits, SDK/NDK and Ninja. Check out the pinned OCCT source in a dependency directory, then run:

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

For an interactive local signing build, set `FSTL_SIGN_WITH_KEYSTORE=1`, `FSTL_KEYSTORE` and `FSTL_KEY_ALIAS`. The deploy tool prompts for passwords. Keep signing files outside the tracked source tree; do not put passwords in scripts, command arguments or workflow files. Preserve the original key and encrypted backups until its role is confirmed.

## Phone development APK

The Android workflow runs on pull requests and defaults to building a second artifact, `fstl-e-phone-arm64-<commit>`. This APK uses `com.github.prjm.fstl_e.dev` and a debug signing key so it installs beside the existing release. Native code is reused from the Release compile; this is a debug Android package, not a native Debug build. CI debug keys are ephemeral, so a later run may not upgrade a previous test installation. Use a persistent local test key for repeated upgrades. Never substitute this artifact for a Play release.

After the local release build, package and install with:

```bash
bash scripts/package_android_phone_apk.sh
export ANDROID_SERIAL="YOUR_DEVICE_SERIAL"
bash scripts/deploy_android_phone.sh build/android-release/phone-artifacts/fstl-e-arm64-dev.apk
```

Packaging requires lint, native alignment, APK signature and ZIP alignment checks, and verifies the separate package ID before exporting. Deployment rejects release package IDs and uses `adb install -r -t`; it does not uninstall or clear app data. Files opened through startup intents or subsequent file-open events retain their complete content URI. STEP/STP filename filters are now included. Provider URIs without extensions and permission persistence still require device testing.

Set `JAVA_HOME` to your JDK 17 installation and `ANDROID_SDK_ROOT` to your Android SDK. Add the JDK and SDK platform tools to `PATH`. Use the tool versions and Qt/OCCT paths documented in the build section.

The CI download can be installed directly with the deployment script after verifying its checksum. For local builds, also set the Qt/OCCT paths from the local build section above.

## Next stage: signed testing delivery

After the unsigned pipeline passes and a phone confirms the build behaves correctly:

1. In Play Console, record the app's current highest version code, upload certificate fingerprint, app-signing certificate fingerprint and testing/production status. The candidate uses versionCode 27; verify every upload and draft, then increase it if needed before Play delivery.
2. Determine whether the local key is the **upload key** or the **app signing key**. Use the existing identity or the appropriate Play reset procedure. Do not generate a replacement key blindly.
3. Add a separate, manually triggered delivery workflow behind a GitHub `play-internal` environment. Restrict it to the protected main branch and approved commits. Configure environment approval if your GitHub plan supports it.
4. Store the upload keystore and passwords as environment secrets, restore the keystore only into the runner's temporary directory, and remove it after use. Public/fork pull-request jobs must never receive those secrets. Use a separate signing step that can read passwords from environment/file inputs without echoing them.
5. Give a Play service account only the app/track permissions required to deliver testing releases. Store its credential as an environment secret or use short-lived federation where supported. Have the workflow upload to the **internal testing track** first. Leave production promotion as a deliberate separate step.
6. Verify installation and upgrades through Play internal testing, inspect the pre-launch report and satisfy any closed-testing gate before applying for production access.

Signed Play delivery is not implemented in the validation workflows. Google's [Play App Signing documentation](https://support.google.com/googleplay/android-developer/answer/9842756) explains the distinction between upload and signing keys. Personal developer accounts created after November 13, 2023 require at least 12 closed testers continuously opted in for 14 days before applying for production access; internal testing does not fulfill that requirement. [Official testing requirements](https://support.google.com/googleplay/android-developer/answer/14151465).

## Phone testing

On the development computer with Android platform tools installed, connect a test phone with USB debugging enabled and accept its authorization prompt. Confirm it appears with `adb devices -l`. Keep device serials, local installation paths and detailed workstation inventories in private validation records. Record the app version, Android/API version, architecture and page size when reporting compatibility results. Test the reviewed candidate build rather than relying on an older installation.

Test cold launch, settings, rotation/zoom, STL/3MF/STEP imports through local and cloud file providers, malformed files, reload/restart, screenshots, background/foreground, second file-open intents, Android 16 insets and a 16 KB device/emulator. Preserve reference screenshots of the current GUI.
