# Contributing

## Scope and style

Keep each change focused on a concrete behavior or documented development need. Follow the surrounding C++, CMake, shell and Python style; avoid unrelated formatting churn. Preserve copyright notices, upstream attribution and third-party license obligations.

Use relative repository links in documentation and placeholder SDK/dependency paths in examples. Document tool versions, required inputs, commands and expected outputs. Separate observed validation results from pending checks; do not claim universal compatibility or production readiness from one device or a static scan.

## Local validation

Install the tools and dependencies listed in [CI/CD](docs/CI_CD.md). From the repository root:

```bash
shellcheck scripts/*.sh tests/*.sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash tests/run_loader_security.sh
```

For Android changes, build and validate the bundle as documented in CI/CD, then exercise affected flows on a test device. Use synthetic malformed and valid models when checking import handling. Verify expected geometry, failure recovery and resource cleanup. Record Android/API version, architecture and page size when reporting compatibility.

Before submitting, run `git diff --check`, review the staged diff, and verify documentation links and fenced commands. Describe the behavior change and relevant validation in the PR. Keep runtime and release limitations visible to reviewers.

## Privacy and artifacts

Commit specific reviewed paths. Keep signing material, credentials, account-specific Console records, device serials, personal workstation paths, confidential models and unrelated logs out of tracked files and GitHub discussions. Use GitHub's noreply email for commits when a personal address should remain private. Keep detailed validation records locally.

CI runs a pinned, checksummed Gitleaks binary against reachable source history. The repository configuration extends credential checks with workstation-path and device-serial checks. Before publishing, run the same checks locally with redacted output:

```bash
gitleaks git . --config .gitleaks.toml --log-opts="--all" --redact=100
```

This source-history scan does not inspect compiled APK contents, commit-email metadata or GitHub cached PR references. Review the current tree, reachable history, commit metadata and artifact contents; deleting a file does not erase its previous versions. Pattern scans cannot prove the absence of secrets. Native libraries can retain build paths even after debug symbols are stripped.

Generate APKs/AABs in ignored build directories. Publish only reviewed artifacts with checksums and clear testing/release status. Development APKs and unsigned bundles are not Play delivery artifacts. Preserve branch protections and required checks; coordinate history cleanup before updating shared branches or released tags.

## Releases

Follow [Play readiness](docs/PLAY_READINESS.md), [Android compatibility](docs/ANDROID_COMPATIBILITY.md) and [testing program guidance](docs/TESTING_PROGRAM.md). Validate the final signed bundle, signing identity, version-code floor, dependency notices and applicable device coverage before widening access. Production promotion is a separate reviewed action.
