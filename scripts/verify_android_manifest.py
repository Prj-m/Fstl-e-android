#!/usr/bin/env python3
"""Check the release manifest dumped from the actual AAB by bundletool."""
import argparse
import sys
import xml.etree.ElementTree as ET

ANDROID = "{http://schemas.android.com/apk/res/android}"
PACKAGE = "com.github.prjm.fstl_e"
ACTIVITY = "org.qtproject.qt.android.bindings.QtActivity"


def verify_manifest(xml, highest_version_code=None):
    root = ET.fromstring(xml)
    if root.tag != "manifest" or root.get("package") != PACKAGE:
        raise ValueError("Unexpected release package ID")
    version = int(root.get(ANDROID + "versionCode", "0"))
    if version <= 0 or (highest_version_code is not None and version <= highest_version_code):
        raise ValueError("Release version code must exceed the highest Play upload")
    sdk = root.find("uses-sdk")
    if sdk is None or int(sdk.get(ANDROID + "targetSdkVersion", "0")) < 36:
        raise ValueError("Release must target API 36 or higher")
    if int(sdk.get(ANDROID + "minSdkVersion", "0")) < 28:
        raise ValueError("Manifest minimum SDK must match the native API 28 build floor")
    applications = root.findall("application")
    if len(applications) != 1:
        raise ValueError("Expected one application")
    app = applications[0]
    if app.get(ANDROID + "debuggable", "false") != "false":
        raise ValueError("Release must not be debuggable")
    for attribute in ("allowBackup", "usesCleartextTraffic"):
        if app.get(ANDROID + attribute) != "false":
            raise ValueError(f"Release must explicitly disable {attribute}")
    for permission in list(root.findall("uses-permission")) + list(root.findall("uses-permission-sdk-23")):
        name = permission.get(ANDROID + "name", "")
        if name in ("android.permission.READ_EXTERNAL_STORAGE", "android.permission.WRITE_EXTERNAL_STORAGE"):
            if int(permission.get(ANDROID + "maxSdkVersion", "999")) > 32:
                raise ValueError("Legacy storage permissions must stop at API 32")
        elif name != PACKAGE + ".DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION":
            raise ValueError(f"Permission requires review for this offline viewer: {name}")
    viewer_activities = [a for a in app.findall("activity") if a.get(ANDROID + "name") == ACTIVITY]
    if len(viewer_activities) != 1:
        raise ValueError("Expected exactly one viewer activity")
    # Qt supports one running activity per process. With singleTop, a VIEW
    # intent sent from another app's task creates a second instance and Qt
    # restarts the process without the file. singleTask routes the intent to
    # the running instance (bundletool dumps the enum as its integer, 2).
    if viewer_activities[0].get(ANDROID + "launchMode") not in ("singleTask", "2"):
        raise ValueError("Viewer activity must use launchMode singleTask so file intents reach the running instance")
    # Freeform <layout> defaults were applied by some launchers when the task
    # was started from another app, and the smaller task bounds persisted
    # across relaunches, leaving the bottom of the screen blank.
    if viewer_activities[0].find("layout") is not None:
        raise ValueError("Viewer activity must not declare freeform layout defaults")
    for tag in ("activity", "activity-alias", "service", "receiver", "provider"):
        for component in app.findall(tag):
            exported = component.get(ANDROID + "exported")
            if exported == "true" or (exported is None and component.find("intent-filter") is not None):
                viewer = tag == "activity" and component.get(ANDROID + "name") == ACTIVITY
                privileged_profiler = (tag == "receiver" and
                    component.get(ANDROID + "name") == "androidx.profileinstaller.ProfileInstallReceiver" and
                    component.get(ANDROID + "permission") == "android.permission.DUMP")
                if not viewer and not privileged_profiler:
                    raise ValueError("Unexpected exported component")
    return version


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest")
    parser.add_argument("--highest-version-code", type=int)
    args = parser.parse_args()
    try:
        with open(args.manifest, encoding="utf-8") as file:
            version = verify_manifest(file.read(), args.highest_version_code)
    except (ValueError, OSError, ET.ParseError) as error:
        print(f"Release manifest verification failed: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"Verified release manifest; version code {version}.")
