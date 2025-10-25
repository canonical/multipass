(reference-settings-local-driver)=
# local.driver

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [Driver](/explanation/driver), [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver)

## Key

`local.driver`

## Description

A string identifying the hypervisor back-end in use.

## Possible values

  - `qemu` on Linux
  - `hyperv` and `virtualbox` on Windows
  - `qemu` and `virtualbox` on macOS 10.15+

## Default values

  - `qemu` on macOS and Linux
  - `hyperv` on Windows
