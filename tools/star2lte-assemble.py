#!/usr/bin/env python3
"""Assemble the star2lte XNU artifacts into an Odin-flashable AP tar.

  star2lte-assemble.py --kernel KERNEL --adt ADT --loader IMAGE \
      [--template STOCK_BOOT] --out-dir DIR [--cmdline "..."] \
      [--boot-args "..."] [--partition-size N] [--dry-run]

The kernel is the CI artifact `kernel.development.star2lte`; the ADT is the
Apple device tree; IMAGE is the star2lte xnu_boot loader (a raw arm64 Image);
the template is a stock OneUI boot.img that supplies the Samsung header, DTBs
and ramdisk. Steps:

  1. validate the inputs and show sizes/addresses;
  2. run tools/mkpayload.py -> payload.bin (the SWEETXNU container);
  3. run tools/star2lte-pack-boot.py pack TEMPLATE IMAGE boot.img
     --append payload.bin, which copies the stock arm64 Image header, DTBH and
     ramdisk;
  4. write star2lte-xnu-boot.tar (USTAR) containing only boot.img for Odin's
     AP slot.

Only those three files are written, all inside --out-dir. The tool never opens
a block device. Keep the stock boot image, the packed image and the tar off
Git: they may contain private device data.
"""
import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tarfile

REPO_ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR = Path(__file__).resolve().parent
MKPAYLOAD = TOOLS_DIR / "mkpayload.py"
PACKER = TOOLS_DIR / "star2lte-pack-boot.py"

DEFAULT_TEMPLATE = REPO_ROOT / "research" / "star2lte" / "oneui-boot.img"
DEFAULT_CMDLINE = (
    "debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress"
)
DEFAULT_PARTITION_SIZE = 0x4000000	# star2lte BOOT partition: 64 MiB
PAYLOAD_NAME = "payload.bin"
BOOT_NAME = "boot.img"
TAR_NAME = "star2lte-xnu-boot.tar"
BOOT_LINE_LENGTH = 608			# mkpayload payload header field

TEMPLATE_HINT = (
    "The stock OneUI BOOT image is gitignored and is not in a fresh clone.\n"
    "  Extract boot.img from a G965FXXUHFVG4 (or your phone's) AP firmware\n"
    "  (boot.img.lz4 -> `lz4 -d boot.img.lz4 boot.img`) and put it at\n"
    "  research/star2lte/oneui-boot.img, or pass --template <your boot.img>.\n"
    "  See STAR2LTE_PROGRESS.md, section 'Stage 0: handoff probe'."
)
LZ4_MAGIC = b"\x04\x22\x4d\x18"


def die(message):
    raise SystemExit(f"star2lte-assemble: error: {message}")


def warn(message):
    print(f"star2lte-assemble: warning: {message}", file=sys.stderr)


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, str(path))
    if spec is None or spec.loader is None:
        die(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def human_size(size):
    return f"{size:,} bytes ({size / 1048576:.2f} MiB)"


def format_command(command):
    values = [str(value) for value in command]
    if os.name == "nt":
        return subprocess.list2cmdline(values)
    return shlex.join(values)


def run(command, dry_run):
    print("$ " + format_command(command), flush=True)
    if dry_run:
        return
    try:
        subprocess.run(command, check=True)
    except FileNotFoundError:
        die(f"cannot execute {command[0]}")
    except subprocess.CalledProcessError as exc:
        die(f"command failed with exit status {exc.returncode}")


def require_file(path, label):
    if not path.exists():
        if label == "template":
            die(f"template boot image not found: {path}\n{TEMPLATE_HINT}")
        die(f"{label} not found: {path}")
    if not path.is_file():
        die(f"{label} is not a file: {path}")
    if path.stat().st_size == 0:
        die(f"{label} is empty: {path}")


def git(*args):
    try:
        return subprocess.run(
            ["git", "-C", str(REPO_ROOT), *args],
            capture_output=True, text=True)
    except FileNotFoundError:
        die("git is not installed; cannot verify that the template is kept "
            "out of Git. Move the template outside the repository.")


def check_template_location(template):
    """Refuse a template that Git would commit (tracked, or inside the repo
    and not ignored). research/ and *.img are ignored, so the stock image in
    research/star2lte/ is fine; a copy next to the sources is not."""
    try:
        relative = template.relative_to(REPO_ROOT)
    except ValueError:
        return
    relative = relative.as_posix()
    tracked = git("ls-files", "--error-unmatch", "--", relative)
    if tracked.returncode == 0:
        die(f"template is tracked by Git: {template}\n"
            "The stock BOOT image may hold private device data. Move it out "
            "of the repository or add its path to .gitignore.")
    if tracked.returncode not in (0, 1):
        die(f"cannot ask git whether the template is tracked: "
            f"{(tracked.stderr or '').strip()}")
    ignored = git("check-ignore", "-q", "--", relative)
    if ignored.returncode == 0:
        return
    if ignored.returncode == 1:
        die(f"template is inside the repository but not gitignored: {template}\n"
            "A stock BOOT image must not be committed. Put it in "
            "research/star2lte/ (ignored) or outside the repository.")
    die(f"cannot ask git whether the template is ignored: "
        f"{(ignored.stderr or '').strip()}")


def check_out_dir(out_dir, inputs, outputs):
    if out_dir.exists() and not out_dir.is_dir():
        die(f"--out-dir exists and is not a directory: {out_dir}")
    try:
        out_dir.relative_to(REPO_ROOT)
    except ValueError:
        pass
    else:
        print(f"note: --out-dir is inside the repository ({out_dir});")
        print("      the .tar and boot.img are not gitignored, so keep them "
              "out of commits.")
    resolved_inputs = {path.resolve() for path in inputs}
    for output in outputs:
        if output.parent.resolve() != out_dir.resolve():
            die(f"refusing to write outside --out-dir: {output}")
        if output.resolve() in resolved_inputs:
            die(f"output would overwrite an input: {output}")
        if output.exists() and not output.is_file():
            die(f"output path exists and is not a file: {output}")


def parse_args(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--kernel", type=Path, required=True,
                    help="kernel.development.star2lte from the xnu workflow")
    ap.add_argument("--adt", type=Path, required=True,
                    help="star2lte Apple device tree (dram-base/dram-size)")
    ap.add_argument("--loader", type=Path, required=True,
                    help="raw arm64 Image of the star2lte xnu_boot loader")
    ap.add_argument("--template", type=Path, default=DEFAULT_TEMPLATE,
                    help="stock OneUI boot.img (default: "
                         "research/star2lte/oneui-boot.img)")
    ap.add_argument("--out-dir", type=Path, required=True,
                    help="directory for payload.bin, boot.img and the AP tar")
    ap.add_argument("--cmdline", default=DEFAULT_CMDLINE,
                    help="full XNU boot command line passed to mkpayload.py "
                         "(default: herolte console/root arguments)")
    ap.add_argument("--boot-args", default="",
                    help="extra XNU boot arguments appended to --cmdline")
    ap.add_argument("--partition-size", type=int, default=DEFAULT_PARTITION_SIZE,
                    help="BOOT partition size for the fit check; 0 disables "
                         "(default: 0x4000000 = 64 MiB)")
    ap.add_argument("--dry-run", action="store_true",
                    help="validate and print the commands without writing")
    return ap.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    kernel = args.kernel.expanduser().resolve()
    adt = args.adt.expanduser().resolve()
    loader = args.loader.expanduser().resolve()
    template = args.template.expanduser().resolve()
    out_dir = args.out_dir.expanduser().resolve()

    payload_path = out_dir / PAYLOAD_NAME
    boot_path = out_dir / BOOT_NAME
    tar_path = out_dir / TAR_NAME
    inputs = (kernel, adt, loader, template)
    outputs = (payload_path, boot_path, tar_path)

    check_out_dir(out_dir, inputs, outputs)

    print("star2lte-assemble: XNU BOOT image -> Odin AP tar")
    print(f"  out-dir  {out_dir}")
    print(f"  template {template}")
    require_file(template, "template")
    check_template_location(template)
    require_file(kernel, "kernel")
    require_file(adt, "ADT")
    require_file(loader, "loader")

    mkpayload = load_module(MKPAYLOAD, "star2lte_mkpayload")
    packer = load_module(PACKER, "star2lte_pack_boot")

    # Kernel: parse the Mach-O here so a corrupt CI artifact fails before any
    # output is written (mkpayload.py does the actual fixup and packing).
    kernel_data = kernel.read_bytes()
    try:
        macho = mkpayload.parse_macho(kernel_data)
    except Exception as exc:
        die(f"kernel is not a 64-bit Mach-O: {exc}")
    if not macho["segs"]:
        die("kernel has no loadable segments")
    text = macho["segs"][0]
    print(f"  kernel   {kernel}  {human_size(len(kernel_data))}")
    print(f"           Mach-O entry={macho['entry']:#x} "
          f"__TEXT vmaddr={text['vmaddr']:#x} "
          f"vmsize={text['vmsize']:#x}")

    # ADT: mkpayload reads these two values for phys_base/mem_size.
    adt_blob = adt.read_bytes()
    try:
        dram_base, dram_size = mkpayload.adt_dram(adt_blob)
    except Exception as exc:
        die(f"ADT lacks /chosen dram-base/dram-size: {exc}")
    print(f"  adt      {adt}  {human_size(len(adt_blob))}")
    print(f"           dram-base={dram_base:#x} dram-size={dram_size:#x} "
          f"(end={dram_base + dram_size:#x})")

    # Loader: must be the raw arm64 Image the packer appends the payload to.
    loader_data = loader.read_bytes()
    if len(loader_data) < 64 or loader_data[56:60] != b"ARMd":
        die(f"loader is not a raw arm64 Image (missing 'ARMd' magic at "
            f"offset 56): {loader}")
    text_offset, image_size, flags = packer.image_header(loader_data)
    print(f"  loader   {loader}  {human_size(len(loader_data))}")
    print(f"           arm64 Image text_offset={text_offset:#x} "
          f"image_size={image_size:#x} flags={flags:#x}")

    # Template: the packer copies the Samsung header, DTBs and ramdisk.
    template_data = template.read_bytes()
    if template_data[:4] == LZ4_MAGIC:
        die(f"template is LZ4-compressed: {template}\n"
            "Decompress it first, e.g. `lz4 -d boot.img.lz4 boot.img`, and "
            "pass the raw boot.img.")
    try:
        stock = packer.parse(template_data)
    except SystemExit as exc:
        die(f"template is not an Android BOOT image: {exc}")
    version, entries = packer.dtbh_entries(stock["dt"])
    chips = sorted({entry[0] for entry in entries})
    if version is None:
        die("template's DT does not start with a DTBH table")
    if 9810 not in chips:
        die(f"template has no Exynos 9810 DTBH entry (chips="
            f"{', '.join(hex(chip) for chip in chips) or 'none'}); this is "
            "not a star2lte boot image")
    print(f"  template {template}  {human_size(len(template_data))}")
    print(f"           page={stock['page']} "
          f"kernel_addr={stock['kernel_addr']:#x} "
          f"ramdisk_addr={stock['ramdisk_addr']:#x} "
          f"dtbh_chips={', '.join(hex(chip) for chip in chips)}")

    cmdline = args.cmdline.strip()
    if args.boot_args.strip():
        cmdline = (cmdline + " " + args.boot_args.strip()).strip()
    if not cmdline:
        die("--cmdline/--boot-args produced an empty command line")
    if len(cmdline.encode()) > BOOT_LINE_LENGTH - 1:
        die(f"boot command line is {len(cmdline.encode())} bytes; the payload "
            f"header holds at most {BOOT_LINE_LENGTH - 1}")
    print(f"  cmdline  {cmdline}")

    mkpayload_cmd = [
        sys.executable, str(MKPAYLOAD), str(kernel), str(adt), str(payload_path),
        "--cmdline", cmdline,
    ]
    pack_cmd = [
        sys.executable, str(PACKER), "pack", str(template), str(loader),
        str(boot_path), "--append", str(payload_path),
    ]
    if args.partition_size:
        pack_cmd += ["--partition-size", str(args.partition_size)]

    if args.dry_run:
        print("dry run: no directory is created and nothing is written")
    else:
        out_dir.mkdir(parents=True, exist_ok=True)
        if boot_path.exists():
            # star2lte-pack-boot.py deliberately refuses to overwrite.
            try:
                boot_path.unlink()
            except OSError as exc:
                die(f"cannot replace {boot_path}: {exc}")

    run(mkpayload_cmd, args.dry_run)
    run(pack_cmd, args.dry_run)

    # The packer writes no tar; Odin wants one top-level boot.img in a plain
    # (uncompressed) tar. USTAR is the most conservative format Odin accepts.
    print(f"[internal] USTAR {tar_path} <= boot.img ({boot_path})")
    if not args.dry_run:
        info = tarfile.TarInfo("boot.img")
        info.size = boot_path.stat().st_size
        info.mode = 0o644
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.mtime = 0
        try:
            with tarfile.open(tar_path, "w",
                              format=tarfile.USTAR_FORMAT) as archive:
                with open(boot_path, "rb") as stream:
                    archive.addfile(info, stream)
        except OSError as exc:
            die(f"cannot write {tar_path}: {exc}")

    if args.dry_run:
        print()
        print("dry run complete: nothing was written.")
        return 0

    missing = [path for path in outputs if not path.exists()]
    if missing:
        die("expected output missing: " + ", ".join(str(path) for path in missing))

    print()
    print("artifacts:")
    for path in outputs:
        print(f"  {path}")
        print(f"      {human_size(path.stat().st_size)}  "
              f"sha256={sha256_file(path)}")
    try:
        candidate = packer.parse(boot_path.read_bytes())
        print(f"packed boot: kernel={len(candidate['kernel'])} B at "
              f"{candidate['kernel_addr']:#x}, "
              f"ramdisk={len(candidate['ramdisk'])} B, "
              f"dt={len(candidate['dt'])} B")
    except SystemExit:
        pass
    print()
    print(f"Flash this via Odin AP: {tar_path}")
    print("The tar contains only boot.img, so Odin writes the BOOT partition "
          "only.")
    if args.partition_size:
        print(f"BOOT fit check: {args.partition_size:,} bytes "
              f"({args.partition_size / 1048576:.0f} MiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
