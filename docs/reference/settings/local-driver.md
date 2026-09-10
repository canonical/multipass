(reference-settings-local-driver)=
# local.driver

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [Driver](/explanation/driver), [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver)

## Key

`local.driver`

## Description

A string identifying the hypervisor back-end in use.

## Possible values

  - `qemu` on Linux
  - `hyperv_api`, `hyperv` (deprecated) and `virtualbox` (deprecated) on Windows
  - `qemu` and `applevz` on macOS; `virtualbox` (deprecated) on macOS running on Intel/x86 only

## Default values

  - `qemu` on macOS and Linux
  - `hyperv_api` on Windows

```{note}
As of Multipass 1.17, `hyperv` and `virtualbox` are deprecated. See [Migrate from Hyper-V to the new Hyper-V API driver on Windows](/how-to-guides/customise-multipass/migrate-from-hyperv-to-hyperv-api-on-windows) and [Move from VirtualBox to another driver](/how-to-guides/customise-multipass/move-from-virtualbox-to-another-driver).
```
