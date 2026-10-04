#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" != 1 || ! -s "$1" ]]; then
    echo "Usage: ANDROID_SERIAL=<phone> bash scripts/deploy_android_phone.sh <dev.apk>" >&2
    exit 1
fi
: "${ANDROID_SERIAL:?Set ANDROID_SERIAL from adb devices -l}"
[[ "$(adb -s "$ANDROID_SERIAL" get-state)" == device ]]
: "${ANDROID_SDK_ROOT:?Set ANDROID_SDK_ROOT for APK identity/signature checks}"
sdk_tools="$ANDROID_SDK_ROOT/build-tools/36.0.0"
"$sdk_tools/apksigner" verify "$1"
"$sdk_tools/aapt" dump badging "$1" | python3 -c '
import sys
if "package: name=\x27com.github.prjm.fstl_e.dev\x27" not in sys.stdin.read():
    sys.exit("Only the separate fstl-e .dev APK may be installed by this script")
'
adb -s "$ANDROID_SERIAL" install -r -t "$1"
adb -s "$ANDROID_SERIAL" shell am start -W -n \
    com.github.prjm.fstl_e.dev/org.qtproject.qt.android.bindings.QtActivity
adb -s "$ANDROID_SERIAL" shell pidof com.github.prjm.fstl_e.dev
echo "For app logs: adb -s $ANDROID_SERIAL logcat --pid=<PID printed above>"
