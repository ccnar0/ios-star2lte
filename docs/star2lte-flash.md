# Trying iOS 16 on the Galaxy S9+ (SM-G965F/DS)

Experimental. This replaces the **BOOT** partition and is flashed with **Odin**
on a PC — no TWRP is required. You need a flat **SM-G965F/DS (Exynos 9810)**,
an unlocked bootloader (OEM unlock), Odin3 with the Samsung USB driver, and a
USB cable. Other S9/S9+ variants (Snapdragon, Exynos 9810 `starlte` S9,
carrier models with a different partition layout) are untested.

Apple's iOS userland is **not** in this repository and must not be uploaded
publicly. You build your own root image from Apple's public IPSW in a
**private** repository of your own.

> Status: the port is at the BOOT/loader stage. The pipeline below produces the
> right Odin artifact, but expect a loader banner first, not a booting iOS.

## 1. Prerequisites

- A **SM-G965F/DS** with an unlocked bootloader. Check the model in
  **Settings -> About phone** before flashing anything.
- Odin3 and the Samsung USB driver installed on Windows.
- Python 3.8+ on the PC.
- A stock star2lte `boot.img` as the template. The tested one is the OneUI 11
  `G965FXXUHFVG4` image (`research/star2lte/oneui-boot.img`, gitignored). To
  obtain it, extract `boot.img.lz4` from the AP firmware and decompress it:

  ```sh
  lz4 -d boot.img.lz4 boot.img
  ```

  If the template is missing, `tools/star2lte-assemble.py` prints these
  instructions instead of failing with a traceback.
- The stock undo tar, `research/star2lte/star2lte-stock-boot.tar`. It is the
  untouched `boot.img.lz4` (or `boot.img`) from the same AP firmware, in a tar
  with that file as its only member:

  ```sh
  tar -cf research/star2lte/star2lte-stock-boot.tar -C /path/to/ap boot.img.lz4
  ```

  Keep the firmware, the template and every assembled file out of Git:
  `research/` and `*.img` are ignored, but a `.tar` elsewhere is not.

## 2. CI artifacts

### Kernel — `xnu` workflow

1. Fork this repository and enable Actions.
2. Run the workflow by hand with the star2lte machine config:

   ```sh
   gh workflow run xnu.yml -R <your-fork> \
     -f machine_config=STAR2LTE -f kernel_config=DEVELOPMENT
   ```

3. Download the kernel artifact. The default runner is `macos-15-intel`, so the
   artifact is **`xnu-kernel-macos-15-intel`** (if you pass another runner, the
   name follows it, e.g. `xnu-kernel-macos-14`). It contains
   `kernel.development.star2lte` and `SHA256SUMS`.

   ```sh
   gh run download -R <your-fork> -n xnu-kernel-macos-15-intel -D artifacts/kernel
   ```

### Probe — `star2lte-probe` workflow

The stage-0 handoff probe is built on Ubuntu and uploaded as the
**`star2lte-probe`** artifact, containing `probe.Image`:

```sh
gh workflow run star2lte-probe.yml -R <your-fork>
gh run download -R <your-fork> -n star2lte-probe -D artifacts/probe
```

`probe.Image` is **not** the XNU loader. It only proves that S-Boot boots a
raw arm64 Image and reports the handoff. Use it if the XNU loader path is in
doubt.

### Loader — `shim/star2lte/out/xnu_boot.Image`

The XNU loader is built locally from `shim/star2lte/` with an aarch64 GCC
(`aarch64-linux-gnu-gcc` or `aarch64-none-elf-gcc`):

```sh
cd shim/star2lte
CROSS=aarch64-none-elf- sh build.sh
cd ../..
```

The loader must be the raw arm64 Image, not an ELF. `build.sh` builds
`out/xnu_boot.Image` (and the probe/preflight diagnostics) and enforces the
`ARMd` header and `_image_end` alignment. Do not point `--loader` at an ELF.

### ADT — `star2lte.adt`

The payload's `/chosen` node decides the kernel's physical RAM window
(`dram-base`/`dram-size`), and the loader refuses a window outside its
star2lte ranges. Build the star2lte ADT with the star2lte ADT generator
(`tools/mkadt_star2lte.py`, tracked in `AGENTS.md`; do **not** reuse
`mkadt_herolte.py` or the S7 addresses). Put the result in
`research/star2lte/star2lte.adt` (ignored) or pass any path with `--adt`.

## 3. Assemble the Odin AP tar

From the repository root:

```sh
python tools/star2lte-assemble.py \
  --kernel artifacts/kernel/kernel.development.star2lte \
  --adt research/star2lte/star2lte.adt \
  --loader shim/star2lte/out/xnu_boot.Image \
  --template research/star2lte/oneui-boot.img \
  --out-dir research/star2lte/ap
```

The tool:

1. validates the inputs and prints sizes and hex addresses;
2. runs `tools/mkpayload.py` to make `payload.bin` (`SWEETXNU` container);
3. runs `tools/star2lte-pack-boot.py pack ... --append payload.bin` against
   the stock template, so the Samsung header, DTBH and stock ramdisk are kept;
4. writes `star2lte-xnu-boot.tar` containing only `boot.img`, in the plain
   (uncompressed) tar form Odin's AP slot expects.

Outputs in `research/star2lte/ap/`:

| File | Purpose |
| --- | --- |
| `payload.bin` | kernel + ADT, appended to the loader |
| `boot.img` | Samsung BOOT image, useful for TWRP/`dd` flashing |
| `star2lte-xnu-boot.tar` | **flash this** as Odin AP |

The last lines print the SHA-256 of each file and the `Flash this via Odin AP:`
path. Keep the SHA-256 with the artifact.

The default XNU command line is intentionally minimal for the first boot — one
CPU, console only, no root device (there is no star2lte UFS driver yet):

```text
debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress
```

Use `--cmdline "..."` to replace it wholesale, or `--boot-args "..."` to append
extra flags without retyping the defaults (for example
`--boot-args "debug=0x144"`). `--partition-size` defaults to `0x4000000`
(64 MiB, the star2lte BOOT partition); `0` disables the fit check.
`--dry-run` validates the inputs and prints the exact `mkpayload.py` and
`star2lte-pack-boot.py` commands without writing.

## 4. Flash with Odin

1. Power the phone off.
2. Enter **Download mode**: hold **Power + Volume Down + Bixby**, then press
   **Volume Up** at the warning screen.
3. Connect USB. Odin shows a blue **COM** entry.
4. In Odin, click **AP** and select **`star2lte-xnu-boot.tar`**.
   Leave **BL**, **CP** and **CSC** empty.
5. Keep the default options (**Auto Reboot**, **F. Reset Time**). Do **not**
   enable **Re-Partition**, **Nand Erase** or **EFS Clear**.
6. Press **Start** and wait for the green **PASS**. The phone reboots by
   itself.

Only the BOOT partition is written, because the tar contains only `boot.img`.
Never put a bootloader, modem, recovery, PARAM or EFS file in this flow. The
stock undo tar is flashed exactly the same way (AP slot, nothing else).

In TWRP, the same image can be written with `dd` if you prefer:

```sh
adb push boot.img /tmp/boot.img
adb shell "dd if=/tmp/boot.img of=/dev/block/by-name/boot bs=1048576 conv=fsync"
```

Confirm the real `by-name/boot` path first; the S9+ uses UFS, not the S7's
`155a0000.ufs` path.

## 5. What you should see

The loader draws its banner and status on the S-Boot framebuffer
(`0xcc000000`, 1440x2960 ARGB8888). **The top half of this phone's panel is
damaged**: text above roughly **y = 1480** may be black or unreadable even
when the loader is running. The probe was moved to `TOP=1560` for this reason;
expect the useful lines in the lower half only.

Expected loader lines, in order:

```text
STAR2LTE XNU LOADER
RAM BASE 0x........
COPYING KERNEL
ENTRY 0x........
BOOTARGS 0x........
JUMP XNU
```

A loader failure prints `LOADER FAIL:` and a short reason, then either waits
for TWRP or resets through the Exynos 9810 PMU. That reset is deliberate, not a
crash loop.

The loader also keeps a log page at physical **`0x8ffff000`** (below the XNU
RAM window):

| Offset | Contents |
| --- | --- |
| `0x8ffff000` | 8-byte ASCII `S2XNU001` |
| `+0x008` | `CurrentEL` at entry |
| `+0x010` | `SCTLR_EL1` |
| `+0x018` | FDT pointer from S-Boot (`0x8a000000` on the tested phone) |
| `+0x020` | loader image address |
| `0x8ffff100` | loader writes `0x5354584e` (`NXTS`); XNU overwrites it with its own entry marker |

If a later read-back or debug dump starts with `S2XNU001`, the loader ran.
`NXTS` at `+0x100` means execution stopped before XNU; the XNU marker
(`XNUELTE1` in the herolte/A5XELTE entry series) means XNU's first instruction
ran. The marker proves entry, not MMU setup, fbcon or userland.

Once XNU reaches its framebuffer console, the console text stops when iOS
draws its first frame; that is expected. Keep to the lower half when
photographing the screen.

## 6. Back to Android

Flash **`research/star2lte/star2lte-stock-boot.tar`** as Odin AP, with only the
AP slot filled. It restores the untouched stock BOOT partition; Android should
boot again. If that tar was lost, recreate it from the original AP firmware's
`boot.img.lz4` as shown in section 1.

If you also replaced USERDATA (iOS root image), restore or format Data
separately — flashing BOOT does not touch it.

## 7. Key combinations

| Action | Keys |
| --- | --- |
| Download mode | **Power + Volume Down + Bixby**, then **Volume Up** |
| Recovery (TWRP if installed) | **Power + Volume Up + Bixby** |
| Force reboot | hold **Power + Volume Down** for about 10 s |
| Normal boot | **Power** |

## 8. Troubleshooting

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| Odin says **PASS**, screen stays black | Loader failed before drawing, or you are looking at the damaged half; wrong loader/template | Look at the lower half from an angle; force reboot to Download mode and flash the stock undo tar. Recheck that the template has a chip-9810 DTBH entry and the loader is the star2lte Image. |
| Phone reboots again and again | The loader's failure path resets through the PMU by design; or XNU panicked before fbcon | Photograph the lower half during the first seconds. If there is no text, pack `probe.Image` with the same template to test the BOOT/S-Boot path, then restore stock. |
| Stuck at the loader banner | Payload missing/corrupt, or the ADT RAM window is outside the loader's ranges | Re-run the assembly and check that `payload.bin` starts with `SWEETXNU`, that `/chosen dram-base`/`dram-size` match the star2lte memory map, and that the BOOT fit check passed. |
| Stuck at `COPYING KERNEL` | Kernel segment or `virt_base` does not fit the payload RAM window | Check the ADT against the kernel's Mach-O segments; rebuild with the matching machine config. The loader will normally print a specific `LOADER FAIL:` reason first. |
| Kernel panic before fbcon | XNU machine config, ADT or loader handoff mismatch; no text output yet | Boot the probe first to confirm the handoff, then retry with `--cmdline "debug=0xc serial=3 cpus=1 keepsyms=1"` to isolate SMP. Recheck `STAR2LTE` in `xnu/patches` and the ADT. |
| Odin fails at `SetupConnection` or never shows COM | Driver, cable, port, or phone not in Download mode | Reinstall the Samsung driver, try another USB port/cable, close Kies/Smart Switch, re-enter Download mode, run Odin as administrator. |
| Odin rejects the tar / checksum error | Wrong file, compressed tar, or a tar with extra members | Use the exact `star2lte-xnu-boot.tar`; it must be uncompressed and contain only `boot.img`. Re-run the assembler and compare its SHA-256. |
| Phone boots Android again after PASS | BOOT was not written (wrong slot/file) or the template was the stock boot | Confirm the AP file is the generated tar and the log mentions `boot.img`; re-flash. |

Attach the assembler's output (including the SHA-256 lines), a photo of the
lower half of the screen, and the loader/marker state when reporting a result.
