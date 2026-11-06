#!/usr/bin/env python3
"""Build a TWRP-compatible, read-only STAR2LTE diagnostic ZIP."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo
import stat

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/star2lte-twrp/update-binary"
OUT = ROOT / "out/star2lte-twrp-diagnostic.zip"

def add_file(zf: ZipFile, name: str, data: bytes, mode: int = 0o644) -> None:
    info = ZipInfo(name)
    info.compress_type = ZIP_DEFLATED
    info.external_attr = (stat.S_IFREG | mode) << 16
    zf.writestr(info, data)

def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Missing installer: {SOURCE}")
    payload = SOURCE.read_bytes()
    if not payload.startswith(b"#!/sbin/sh"):
        raise SystemExit("TWRP update-binary must use the recovery shell")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUT, "w") as zf:
        add_file(zf, "META-INF/com/google/android/update-binary", payload, 0o755)
        add_file(zf, "META-INF/com/google/android/updater-script", b"# Custom update-binary performs read-only diagnostics.\n")
        add_file(zf, "README.txt", (
            "STAR2LTE TWRP diagnostic package\n"
            "Target: Samsung Galaxy S9+ Exynos SM-G965F/FD (star2lte)\n"
            "This package is READ-ONLY. It does not flash, mount, format, or erase partitions.\n"
            "Report: /sdcard/star2lte-test-report.txt (or /tmp/star2lte-test-report.txt)\n"
            "Do not use on Snapdragon variants. XNU/iOS is not yet bootable on this target.\n"
        ).encode())
    with ZipFile(OUT) as zf:
        bad = zf.testzip()
        assert bad is None, f"Corrupt ZIP member: {bad}"
        names = set(zf.namelist())
        assert "META-INF/com/google/android/update-binary" in names
        info = zf.getinfo("META-INF/com/google/android/update-binary")
        assert (info.external_attr >> 16) & 0o111, "update-binary is not executable"
    print(f"Created {OUT} ({OUT.stat().st_size} bytes); ZIP integrity and executable bit OK")

if __name__ == "__main__":
    main()
