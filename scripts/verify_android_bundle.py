#!/usr/bin/env python3
"""Check per-ABI app/STEP libraries, 16 KB LOAD alignment and RELRO protection ranges."""
import argparse
import struct
import sys
import zipfile


# ABI -> (ELF class, e_machine)
ABIS = {"arm64-v8a": (2, 183), "armeabi-v7a": (1, 40), "x86_64": (2, 62)}


def verify_elf(data: bytes, name: str, abi: str = "arm64-v8a") -> None:
    elf_class, machine = ABIS[abi]
    # 16 KB page sizes only exist on 64-bit Android; 32-bit libraries need 4 KB.
    page = 16384 if elf_class == 2 else 4096
    if len(data) < 52 or data[:6] != b"\x7fELF" + bytes([elf_class, 1]):
        raise ValueError(f"{name}: expected a little-endian ELF{32 * elf_class} library")
    if struct.unpack_from("<H", data, 18)[0] != machine:
        raise ValueError(f"{name}: expected {abi} machine type")
    if elf_class == 2:
        phoff = struct.unpack_from("<Q", data, 32)[0]
        phentsize, phnum = struct.unpack_from("<HH", data, 54)
        min_entsize = 56
    else:
        phoff = struct.unpack_from("<I", data, 28)[0]
        phentsize, phnum = struct.unpack_from("<HH", data, 42)
        min_entsize = 32
    if phentsize < min_entsize or phnum == 0 or phoff + phentsize * phnum > len(data):
        raise ValueError(f"{name}: invalid ELF program header table")
    loads = 0
    headers = []
    for i in range(phnum):
        if elf_class == 2:
            header = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phentsize)
        else:
            # Reorder ELF32 fields to the ELF64 layout used below.
            kind, offset, vaddr, paddr, filesz, memsz, flags, align = struct.unpack_from(
                "<IIIIIIII", data, phoff + i * phentsize)
            header = (kind, flags, offset, vaddr, paddr, filesz, memsz, align)
        headers.append(header)
        kind, _, offset, vaddr, _, filesz, memsz, align = header
        if kind == 1:
            loads += 1
            if offset + filesz > len(data):
                raise ValueError(f"{name}: truncated load segment")
            if align < page or align & (align - 1) or offset % page != vaddr % page:
                raise ValueError(f"{name}: load segment is not aligned for {page // 1024} KB pages")
    if not loads:
        raise ValueError(f"{name}: no loadable segments")
    for kind, _, _, start, _, _, memsz, _ in headers:
        if kind != 0x6474E552 or memsz == 0:
            continue
        end = start + memsz
        protected_start = start // page * page
        protected_end = (end + page - 1) // page * page
        # Android's linker rounds both RELRO boundaries to whole pages. Padding
        # outside a LOAD is harmless; writable LOAD bytes outside RELRO are not.
        # Checking only end % 16384 rejects safe NDK runtime segment layouts.
        # Reference: AOSP bionic linker/linker_phdr.cpp,
        # _phdr_table_set_gnu_relro_prot (rounds start down and end up).
        for load_kind, flags, _, load_start, _, _, load_size, _ in headers:
            if load_kind != 1 or not flags & 2:
                continue
            overlap_start = max(protected_start, load_start)
            overlap_end = min(protected_end, load_start + load_size)
            if overlap_start < overlap_end and (overlap_start < start or overlap_end > end):
                raise ValueError(f"{name}: 16 KB RELRO protection overlaps writable data")


def verify_bundle(path: str, *, apk: bool = False, abis: tuple = ("arm64-v8a",)) -> int:
    prefix = "lib/" if apk else "base/lib/"
    with zipfile.ZipFile(path) as bundle:
        libraries = [n for n in bundle.namelist() if n.startswith(prefix) and n.endswith(".so")]
        if len(libraries) != len(set(libraries)):
            raise ValueError("Duplicate native-library entries")
        for abi in abis:
            if not any(n.startswith(f"{prefix}{abi}/libfstl_viewer") for n in libraries):
                raise ValueError(f"Missing fstl_viewer {abi} library")
            if f"{prefix}{abi}/libTKDESTEP.so" not in libraries:
                raise ValueError(f"Missing full STEP support for {abi} (libTKDESTEP.so)")
        for name in libraries:
            abi = name[len(prefix):].split("/", 1)[0]
            if abi not in abis:
                raise ValueError(f"Unexpected ABI in build: {name}")
            verify_elf(bundle.read(name), name, abi)
        return len(libraries)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle")
    parser.add_argument("--apk", action="store_true", help="Verify APK native-library paths")
    parser.add_argument("--abis", default="arm64-v8a", help="Expected ABIs, space or comma separated")
    args = parser.parse_args()
    abis = tuple(args.abis.replace(",", " ").split())
    try:
        if not abis or any(abi not in ABIS for abi in abis):
            raise ValueError(f"--abis must be a subset of {', '.join(ABIS)}")
        count = verify_bundle(args.bundle, apk=args.apk, abis=abis)
    except (ValueError, OSError, zipfile.BadZipFile, struct.error) as error:
        print(f"Bundle verification failed: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"Verified {count} {'/'.join(abis)} libraries with page-size LOAD alignment and safe RELRO ranges.")
    print("Device testing and APK packaging/signature verification are still required.")
