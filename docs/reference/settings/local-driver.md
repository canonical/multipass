(reference-settings-local-driver)=
# local.driver

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [Driver](/explanation/driver), [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver)

## Key

`local.driver`

## Description

A string identifying the hypervisor back-end in use.

## Possible values

  - `qemu` on Linux
  - `hcs` and `hyperv` (deprecated) on Windows
  - `qemu` and `applevz` on macOS

## Default values

  - `qemu` on macOS and Linux
  - `hcs` on Windows

```{note}
As of Multipass 1.17, `hyperv` is deprecated. See [Migrate from Hyper-V to the HCS driver on Windows](/how-to-guides/customise-multipass/migrate-from-hyperv-to-hcs-on-windows).
```
