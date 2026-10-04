import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("manifest_check", ROOT / "scripts/verify_android_manifest.py")
manifest_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest_check)

MANIFEST = '''<manifest xmlns:android="http://schemas.android.com/apk/res/android"
 package="com.github.prjm.fstl_e" android:versionCode="20">
 <uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
 <application android:debuggable="false" android:allowBackup="false" android:usesCleartextTraffic="false">
  <activity android:name="org.qtproject.qt.android.bindings.QtActivity" android:exported="true"/>
 </application>
</manifest>'''


class ReleaseManifestChecks(unittest.TestCase):
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

    def test_rejects_exported_service(self):
        xml = MANIFEST.replace('</application>', '<service android:name="Unexpected" android:exported="true"/></application>')
        with self.assertRaises(ValueError):
            manifest_check.verify_manifest(xml)
