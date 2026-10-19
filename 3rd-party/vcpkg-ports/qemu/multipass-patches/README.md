# QEMU patches for Multipass

These patches are applied to upstream QEMU `v<version>` by `../portfile.cmake`
(the `PATCHES` list, in numeric order). This file records what each patch is
for, how much maintenance it needs, and — where applicable — the exact condition
under which it can be dropped.

## The patches

| # | Patch | Notes |
|---|-------|-------|
| 0001 | 9p uid/gid mapping | uid/gid remap for 9p/SSHFS mounts. Carry + rebase until upstreamed. |
| 0002 | revert old highmem=off behavior | Preserves the prior aarch64 `virt` memory map. Carry until the reason is gone / upstream changes. |
| 0003 | zero-initialize vmnet send pos | **Upstream candidate.** Drop when a QEMU version ships the init. |
| 0004 | remap legacy `ID_AA64PFR1_EL1` key | See *Suspend/resume compatibility*. |
| 0005 | disable `-Werror` for dtc subproject | **Upstream candidate** (or adjust build flags). Low effort to carry. |
| 0006 | accept subset feature-ID-register values | See *Suspend/resume compatibility*. |

## Suspend/resume compatibility (0004 + 0006)

Both exist **only** to resume instances suspended by older releases (the QEMU 8.2
era, before these fixes shipped):

- **0004** translates the legacy `ID_AA64PFR1_EL1` HVF sysreg encoding
  (`op2=2`, a bug fixed upstream in commit `19ed42e8`) to the correct key when
  loading CPU state, so the register matches this QEMU's list.
- **0006** accepts read-only AArch64 feature-ID-register values that are a
  field-wise *subset* of the host's. Older QEMU exposed fewer features via
  HVF `-cpu host` (e.g. `FEAT_BTI`, `FEAT_I8MM`), so their suspends record
  smaller values that would otherwise fail write-back on load.

### Retirement checklist (drop 0004 and 0006 together)

Drop both in a release once ALL of the following hold:

1. **No supported suspend predates the fixes.** The oldest Multipass release
   whose suspends we still support was built with QEMU >= 10.0.3 — i.e. no
   supported release used the pre-`19ed42e8` encoding or exposed fewer HVF
   feature-ID bits than 10.0.3.

Dropping is a one-time breaking change only for VMs *still* suspended in the old
format from Multipass v1.16.3; everything else is unaffected. This mirrors the
vendored firmware/ROM deprecation described in `../vendor/README.md` — retire
them in the same release if the timelines line up.
