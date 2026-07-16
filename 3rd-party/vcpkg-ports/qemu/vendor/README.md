# Vendored QEMU firmware / ROM blobs

These files are shipped by `../portfile.cmake` into `Resources/qemu/` instead of
QEMU's own in-tree copies. They exist purely for **backwards compatibility**: a
Multipass instance suspended by an older release records its full QEMU command
line and migration state, and resume replays them verbatim. If a firmware or
option ROM changes size between the release that suspended the instance and the
release that resumes it, QEMU's migration loader aborts and the instance can no
longer be resumed.

Older releases staged QEMU (and its firmware/ROMs) from the Ubuntu archive,
which builds some blobs differently from QEMU's bundled copies. To keep those
suspended instances resumable, we vendor the Ubuntu-compatible blobs here.

> **TODO:** drop these once suspended instances from the affected releases are no
> longer supported (a couple of releases).

## `efi-virtio.rom` (512 KB)

The virtio-net-pci option ROM. QEMU sizes the device's ROM BAR from this file,
rounded up to a power of two:

| Source | Size | ROM BAR |
| --- | --- | --- |
| QEMU in-tree `pc-bios/efi-virtio.rom` | 160768 B | 256 KB (`0x40000`) |
| Ubuntu `ipxe-qemu` `/usr/lib/ipxe/qemu/efi-virtio.rom` (**vendored**) | 524288 B | 512 KB (`0x80000`) |

The correct ROM to ship depends on **which ROM the platform's older releases
used**, because resume happens on the same platform that suspended the instance:

- **Linux** staged the `ipxe-qemu` ROM from the Ubuntu archive, so its suspended
  instances expect a `0x80000` ROM BAR. `portfile.cmake` ships the **vendored**
  512 KB copy on Linux. Shipping QEMU's smaller in-tree ROM there yields a
  `0x40000` BAR and makes resume fail with:

  ```
  qemu-system-x86_64: Size mismatch: 0000:00:03.0/virtio-net-pci.rom: 0x80000 != 0x40000: Invalid argument
  qemu-system-x86_64: error while loading state for instance 0x0 of device 'ram'
  qemu-system-x86_64: Error -22 while loading VM state
  ```

- **macOS** bundled QEMU's in-tree ROM (256 KB), so its suspended instances
  expect a `0x40000` ROM BAR. `portfile.cmake` ships QEMU's **in-tree**
  `pc-bios/efi-virtio.rom` on macOS. Shipping the vendored 512 KB ROM there
  yields a `0x80000` BAR and makes resume fail with the inverse mismatch:

  ```
  qemu-system-aarch64: Size mismatch: 0000:00:01.0/virtio-net-pci.rom: 0x40000 != 0x80000: Invalid argument
  qemu-system-aarch64: error while loading state for instance 0x0 of device 'ram'
  qemu-system-aarch64: Error -22 while loading VM state
  ```

This affects both x86_64 and aarch64, since both use `virtio-net-pci`.

Source: Ubuntu `ipxe-qemu` package
(`/usr/lib/ipxe/qemu/efi-virtio.rom`).

## `OVMF.fd` (2 MB)

The legacy 2 MB combined OVMF image (code + vars). Older releases booted x86_64
instances with `-bios OVMF.fd`; resume replays that command line, so the firmware
must still be locatable and the same size. New instances instead use the split
edk2 firmware loaded via pflash, so this file is only needed for resuming
instances suspended by those older releases.
