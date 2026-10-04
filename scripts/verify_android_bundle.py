#!/usr/bin/env python3
"""Check app/STEP libraries and 16 KB load-segment alignment in an arm64 AAB or APK."""
import argparse
import struct
import sys
import zipfile


def verify_elf(data: bytes, name: str) -> None:
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError(f"{name}: expected a little-endian ELF64 library")
    if struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError(f"{name}: expected AArch64 machine type")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    if phentsize < 56 or phnum == 0 or phoff + phentsize * phnum > len(data):
        raise ValueError(f"{name}: invalid ELF program header table")
    loads = 0
    for i in range(phnum):
        kind, _, offset, vaddr, _, filesz, _, align = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phentsize)
        if kind == 1:
            loads += 1
            if offset + filesz > len(data):
                raise ValueError(f"{name}: truncated load segment")
            if align < 16384 or align & (align - 1) or offset % 16384 != vaddr % 16384:
                raise ValueError(f"{name}: load segment is not aligned for 16 KB pages")
    if not loads:
        raise ValueError(f"{name}: no loadable segments")


def verify_bundle(path: str, *, apk: bool = False) -> int:
    prefix = "lib/" if apk else "base/lib/"
    with zipfile.ZipFile(path) as bundle:
        libraries = [n for n in bundle.namelist() if n.startswith(prefix) and n.endswith(".so")]
        if len(libraries) != len(set(libraries)):
            raise ValueError("Duplicate native-library entries")
        if not any(n.startswith(prefix + "arm64-v8a/libfstl_viewer") for n in libraries):
            raise ValueError("Missing fstl_viewer arm64 library")
        if prefix + "arm64-v8a/libTKDESTEP.so" not in libraries:
            raise ValueError("Missing full STEP support (libTKDESTEP.so)")
        for name in libraries:
            if not name.startswith(prefix + "arm64-v8a/"):
                raise ValueError(f"Unexpected ABI in arm64 build: {name}")
            verify_elf(bundle.read(name), name)
        return len(libraries)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle")
    parser.add_argument("--apk", action="store_true", help="Verify APK native-library paths")
    args = parser.parse_args()
    try:
        count = verify_bundle(args.bundle, apk=args.apk)
    except (ValueError, OSError, zipfile.BadZipFile, struct.error) as error:
        print(f"Bundle verification failed: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"Verified {count} arm64 libraries with 16 KB load-segment alignment.")
    print("Device testing and APK packaging/signature verification are still required.")
