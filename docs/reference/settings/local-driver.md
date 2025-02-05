(reference-settings-local-driver)=
# local.driver

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [Driver](/explanation/driver), [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver)

## Key

`local.driver`

## Description

A string identifying the hypervisor back-end in use.

## Possible values

  - `qemu`, `libvirt` and `lxd` on Linux
  - `hyperv` and `virtualbox` on Windows
  - `qemu` and `virtualbox` on macOS 10.15+
  - *(deprecated)* `hyperkit` on Intel macOS 10.15+

## Default values

  - `qemu` on macOS and AMD64 Linux
  - `lxd` on non-AMD64 Linux
  - `hyperv` on Windows

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/local-driver" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

