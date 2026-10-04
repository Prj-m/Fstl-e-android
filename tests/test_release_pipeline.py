import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bundle_check", ROOT / "scripts/verify_android_bundle.py")
bundle_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle_check)


def elf(align=16384, machine=183, offset=0, vaddr=0):
    data = bytearray(120)
    data[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", data, 18, machine)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HH", data, 54, 56, 1)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, offset, vaddr, 0, 0, 0, align)
    return data


def write_bundle(path, app=True, step=True, alignment=16384, extra=None):
    with zipfile.ZipFile(path, "w") as archive:
        if app:
            archive.writestr("base/lib/arm64-v8a/libfstl_viewer_arm64-v8a.so", elf(alignment))
        if step:
            archive.writestr("base/lib/arm64-v8a/libTKDESTEP.so", elf(alignment))
        if extra:
            archive.writestr(extra, elf(alignment))


class BundleChecks(unittest.TestCase):
    def test_accepts_16k_and_64k_alignment(self):
        for alignment in (16384, 65536):
            with self.subTest(alignment=alignment):
                bundle_check.verify_elf(elf(alignment), "library.so")

    def test_rejects_bad_load_alignment(self):
        for alignment in (0, 4096, 24576):
            with self.subTest(alignment=alignment), self.assertRaises(ValueError):
                bundle_check.verify_elf(elf(alignment), "library.so")

    def test_rejects_noncongruent_load_offsets(self):
        with self.assertRaises(ValueError):
            bundle_check.verify_elf(elf(offset=0, vaddr=4096), "library.so")

    def test_rejects_truncated_headers_and_segments(self):
        damaged = elf()
        struct.pack_into("<Q", damaged, 96, 1000)  # p_filesz
        for data in (b"", elf()[:80], damaged):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                bundle_check.verify_elf(data, "library.so")

    def test_rejects_wrong_architecture(self):
        with self.assertRaises(ValueError):
            bundle_check.verify_elf(elf(machine=62), "library.so")

    def test_rejects_no_load_segments(self):
        data = elf()
        struct.pack_into("<I", data, 64, 2)
        with self.assertRaises(ValueError):
            bundle_check.verify_elf(data, "library.so")

    def test_apk_paths_require_explicit_apk_mode(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "app.apk"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("lib/arm64-v8a/libfstl_viewer_arm64-v8a.so", elf())
                archive.writestr("lib/arm64-v8a/libTKDESTEP.so", elf())
            self.assertEqual(bundle_check.verify_bundle(path, apk=True), 2)
            with self.assertRaises(ValueError):
                bundle_check.verify_bundle(path)

    def test_required_bundle_libraries_and_abi(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "app.aab"
            write_bundle(path)
            self.assertEqual(bundle_check.verify_bundle(path), 2)
            for kwargs in ({"app": False}, {"step": False}, {"alignment": 4096}, {"extra": "base/lib/x86_64/libother.so"}):
                with self.subTest(kwargs=kwargs):
                    write_bundle(path, **kwargs)
                    with self.assertRaises(ValueError):
                        bundle_check.verify_bundle(path)


class ReleaseBuildChecks(unittest.TestCase):
    """Exercise the release coordinator's failure paths with isolated fake tools."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fstle-release-tests-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        tools = self.base / "tools"
        tools.mkdir()
        self.env = os.environ.copy()
        for key in ("FSTL_KEYSTORE", "FSTL_KEY_ALIAS", "FSTL_DEPLOY_JSON", "FSTL_ANDROID_OUTPUT_DIR"):
            self.env.pop(key, None)
        self.env.update({
            "QT_ANDROID_ROOT": str(self.base / "qt/android_arm64_v8a"),
            "QT_HOST_ROOT": str(self.base / "qt/gcc_64"),
            "ANDROID_SDK_ROOT": str(self.base / "sdk"),
            "ANDROID_NDK_ROOT": str(self.base / "ndk"),
            "FSTL_OCCT_ROOT": str(self.base / "occt"),
            "FSTL_ANDROID_BUILD_DIR": str(self.base / "build"),
            "FSTL_SIGN_WITH_KEYSTORE": "0",
            "QT_CMAKE": str(tools / "cmake"),
            "PATH": str(tools) + os.pathsep + self.env["PATH"],
            "FSTL_TEST_ROOT": str(self.base),
        })
        for path in ("qt/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake", "ndk/build/cmake/android.toolchain.cmake", "occt/lib/libTKDESTEP.so", "occt/include/opencascade/STEPControl_Reader.hxx"):
            file = self.base / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.touch()
        (self.base / "sdk/platforms/android-36").mkdir(parents=True)
        write_bundle(self.base / "fixture.aab")
        self.executable(tools / "ninja", "pass\n")
        self.executable(tools / "cmake", """import json, os, pathlib, sys
root = pathlib.Path(os.environ['FSTL_TEST_ROOT'])
with (root / 'cmake-calls').open('a') as log: log.write(json.dumps(sys.argv[1:]) + '\\n')
if os.environ.get('FSTL_TEST_FAIL') == 'native': sys.exit(7)
if '-B' in sys.argv:
    build = pathlib.Path(sys.argv[sys.argv.index('-B') + 1])
    build.mkdir(parents=True, exist_ok=True)
    (build / 'android-fstl_viewer-deployment-settings.json').write_text('{}')
""")
        self.executable(self.base / "qt/gcc_64/bin/androiddeployqt", """import os, pathlib, shutil, sys
root = pathlib.Path(os.environ['FSTL_TEST_ROOT'])
(root / 'deploy-called').touch()
if os.environ.get('FSTL_TEST_FAIL') == 'deploy': sys.exit(8)
out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])
(out / 'gradlew').write_text('exit 9\\n' if os.environ.get('FSTL_TEST_FAIL') == 'lint' else 'exit 0\\n')
if os.environ.get('FSTL_TEST_FAIL') != 'no-bundle':
    bundles = out / 'build/outputs/bundle/release'
    bundles.mkdir(parents=True)
    shutil.copyfile(root / 'fixture.aab', bundles / 'app.aab')
""")

    def executable(self, path, code):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/usr/bin/env python3\n" + code)
        path.chmod(0o755)

    def run_build(self):
        return subprocess.run(["bash", str(ROOT / "scripts/build_android_full_release_aab.sh")], env=self.env, capture_output=True, text=True)

    def prepare_phone_tools(self):
        build = self.base / "build"
        build.mkdir()
        (build / "android-fstl_viewer-deployment-settings.json").write_text("{}")
        with zipfile.ZipFile(self.base / "fixture.apk", "w") as archive:
            archive.writestr("lib/arm64-v8a/libfstl_viewer_arm64-v8a.so", elf())
            archive.writestr("lib/arm64-v8a/libTKDESTEP.so", elf())
        self.executable(self.base / "qt/gcc_64/bin/androiddeployqt", """import os, pathlib, shutil, sys
root = pathlib.Path(os.environ['FSTL_TEST_ROOT'])
assert '--aux-mode' in sys.argv
assert '--install' not in sys.argv and '--release' not in sys.argv
out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])
(out / 'gradlew').write_text('exit 9\\n' if os.environ.get('FSTL_TEST_FAIL') == 'lint' else 'exit 0\\n')
if os.environ.get('FSTL_TEST_FAIL') != 'no-apk':
    apk_dir = out / 'build/outputs/apk/debug'
    apk_dir.mkdir(parents=True)
    shutil.copyfile(root / 'fixture.apk', apk_dir / 'app.apk')
""")
        for tool in ("apksigner", "zipalign", "aapt"):
            self.executable(self.base / "sdk/build-tools/36.0.0" / tool, """import os, sys
if os.environ.get('FSTL_TEST_FAIL') == 'signature': sys.exit(1)
if sys.argv[1] == 'dump':
    package = 'com.github.prjm.fstl_e' if os.environ.get('FSTL_TEST_FAIL') == 'identity' else 'com.github.prjm.fstl_e.dev'
    print("package: name='" + package + "'")
""")

    def run_phone_package(self):
        return subprocess.run(["bash", str(ROOT / "scripts/package_android_phone_apk.sh")], env=self.env, capture_output=True, text=True)

    def test_phone_apk_exports_after_verification(self):
        self.prepare_phone_tools()
        result = self.run_phone_package()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.base / "build/phone-artifacts/fstl-e-arm64-dev.apk.sha256").is_file())

    def test_phone_failures_do_not_export_apk(self):
        self.prepare_phone_tools()
        for failure in ('lint', 'no-apk', 'signature', 'identity'):
            with self.subTest(failure=failure):
                self.env['FSTL_TEST_FAIL'] = failure
                self.assertNotEqual(self.run_phone_package().returncode, 0)
                self.assertFalse((self.base / 'build/phone-artifacts').exists())

    def test_success_exports_a_fresh_bundle_and_checksum(self):
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.base / "build/artifacts/fstl-e-arm64-release.aab.sha256").is_file())
        calls = (self.base / "cmake-calls").read_text()
        self.assertIn("-DCMAKE_BUILD_TYPE=Release", calls)
        self.assertIn("-DFSTL_REQUIRE_OCCT=ON", calls)

    def test_missing_dependency_fails_before_any_build(self):
        (self.base / "occt/lib/libTKDESTEP.so").unlink()
        self.assertNotEqual(self.run_build().returncode, 0)
        self.assertFalse((self.base / "cmake-calls").exists())

    def test_missing_signing_input_fails_before_build(self):
        self.env["FSTL_SIGN_WITH_KEYSTORE"] = "1"
        self.assertNotEqual(self.run_build().returncode, 0)
        self.assertFalse((self.base / "cmake-calls").exists())

    def test_tool_or_lint_failures_do_not_export_artifacts(self):
        for failure in ("native", "deploy", "lint"):
            with self.subTest(failure=failure):
                self.env["FSTL_TEST_FAIL"] = failure
                self.assertNotEqual(self.run_build().returncode, 0)
                self.assertFalse((self.base / "build/artifacts").exists())

    def test_stale_bundle_does_not_mask_missing_output(self):
        stale = self.base / "build/android-release.old/build/outputs/bundle/release"
        stale.mkdir(parents=True)
        write_bundle(stale / "old.aab")
        self.env["FSTL_TEST_FAIL"] = "no-bundle"
        self.assertNotEqual(self.run_build().returncode, 0)
        self.assertFalse((self.base / "build/artifacts").exists())

    def test_bad_native_alignment_prevents_export(self):
        write_bundle(self.base / "fixture.aab", alignment=4096)
        self.assertNotEqual(self.run_build().returncode, 0)
        self.assertFalse((self.base / "build/artifacts").exists())


if __name__ == "__main__":
    unittest.main()
