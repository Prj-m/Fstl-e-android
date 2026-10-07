import importlib.util
from pathlib import Path
import unittest
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("manifest_check", ROOT / "scripts/verify_android_manifest.py")
manifest_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest_check)

MANIFEST = '''<manifest xmlns:android="http://schemas.android.com/apk/res/android"
 package="com.github.prjm.fstl_e" android:versionCode="20">
 <uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
 <application android:debuggable="false" android:allowBackup="false" android:usesCleartextTraffic="false">
  <activity android:name="org.qtproject.qt.android.bindings.QtActivity" android:exported="true" android:launchMode="2"/>
 </application>
</manifest>'''


class ReleaseManifestChecks(unittest.TestCase):
    def test_viewer_and_android_versions_agree(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        project = re.search(r"project\(FstlViewer VERSION ([0-9.]+)", cmake).group(1)
        display = re.search(r'FSTLE_VERSION="([^"\n]+)"', cmake).group(1)
        display = display.replace("${PROJECT_VERSION}", project)
        manifest = ET.parse(ROOT / "android/AndroidManifest.xml").getroot()
        self.assertEqual(display, manifest.get("{http://schemas.android.com/apk/res/android}versionName"))

    def test_valid_release_and_upgrade(self):
        self.assertEqual(manifest_check.verify_manifest(MANIFEST, 19), 20)

    def test_rejects_non_upgrade(self):
        for highest in (20, 21):
            with self.assertRaises(ValueError):
                manifest_check.verify_manifest(MANIFEST, highest)

    def test_rejects_debug_and_test_package(self):
        for xml in (MANIFEST.replace('debuggable="false"', 'debuggable="true"'),
                    MANIFEST.replace('package="com.github.prjm.fstl_e"', 'package="com.github.prjm.fstl_e.dev"')):
            with self.assertRaises(ValueError):
                manifest_check.verify_manifest(xml)

    def test_requires_single_task_viewer(self):
        for mode in ('android:launchMode="1"', 'android:launchMode="singleTop"', ''):
            with self.assertRaises(ValueError):
                manifest_check.verify_manifest(MANIFEST.replace('android:launchMode="2"', mode))
        self.assertEqual(manifest_check.verify_manifest(MANIFEST.replace('android:launchMode="2"', 'android:launchMode="singleTask"')), 20)
        source = ET.parse(ROOT / "android/AndroidManifest.xml").getroot()
        activity = source.find("application/activity")
        self.assertEqual(activity.get("{http://schemas.android.com/apk/res/android}launchMode"), "singleTask")

    def test_rejects_freeform_layout_defaults(self):
        xml = MANIFEST.replace('android:launchMode="2"/>', 'android:launchMode="2"><layout android:defaultWidth="600dp"/></activity>')
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml)
        source = ET.parse(ROOT / "android/AndroidManifest.xml").getroot()
        self.assertIsNone(source.find("application/activity/layout"))

    def test_rejects_browsable_viewer(self):
        xml = MANIFEST.replace('android:launchMode="2"/>', 'android:launchMode="2"><intent-filter><category android:name="android.intent.category.BROWSABLE"/></intent-filter></activity>')
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml)
        source = (ROOT / "android/AndroidManifest.xml").read_text()
        self.assertNotIn("BROWSABLE", source)

    def test_rejects_old_target(self):
        for xml in (MANIFEST.replace('targetSdkVersion="36"', 'targetSdkVersion="35"'),
                    MANIFEST.replace('minSdkVersion="28"', 'minSdkVersion="26"')):
            with self.assertRaises(ValueError):
                manifest_check.verify_manifest(xml)

    def test_rejects_backup_or_cleartext(self):
        for attribute in ('allowBackup', 'usesCleartextTraffic'):
            with self.assertRaises(ValueError):
                manifest_check.verify_manifest(MANIFEST.replace(attribute + '="false"', attribute + '="true"'))

    def test_rejects_unreviewed_permission(self):
        xml = MANIFEST.replace('</manifest>', '<uses-permission android:name="android.permission.INTERNET"/></manifest>')
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml)

    def test_storage_permission_needs_api_cap(self):
        permission = '<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" android:maxSdkVersion="32"/>'
        xml = MANIFEST.replace('</manifest>', permission + '</manifest>')
        self.assertEqual(manifest_check.verify_manifest(xml), 20)
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml.replace(' android:maxSdkVersion="32"', ''))

    def test_profile_receiver_requires_privileged_dump_permission(self):
        receiver = '<receiver android:name="androidx.profileinstaller.ProfileInstallReceiver" android:exported="true" android:permission="android.permission.DUMP"/>'
        xml = MANIFEST.replace("</application>", receiver + "</application>")
        self.assertEqual(manifest_check.verify_manifest(xml), 20)
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml.replace(' android:permission="android.permission.DUMP"', ""))

    def test_rejects_exported_service(self):
        xml = MANIFEST.replace('</application>', '<service android:name="Unexpected" android:exported="true"/></application>')
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml)
