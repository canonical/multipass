---
myst:
  html_meta:
    description: How to choose and set up the Multipass driver on Linux, macOS and Windows, including QEMU, Apple's Virtualization framework (AppleVZ) and Hyper-V.
---

(how-to-guides-customise-multipass-set-up-the-driver)=
# How to set up the driver

> See also: {ref}`explanation-driver`, {ref}`reference-settings-local-driver`

This document demonstrates how to choose, set up, and manage the drivers behind Multipass. Multipass already has sensible defaults, so this is an optional step.

## Default driver

`````{tab-set}

````{tab-item} Linux
:sync: Linux

By default, Multipass on Linux uses the `qemu` driver.

````

````{tab-item} macOS
:sync: macOS

By default, Multipass on macOS uses the `qemu` driver.

````

````{tab-item} Windows
:sync: Windows

By default, Multipass on Windows uses the `hcs` driver.

````

`````

## Install an alternative driver

`````{tab-set}

````{tab-item} Linux
:sync: Linux

On Linux, only the `qemu` driver is supported, so switching `local.driver` is not possible.

````

````{tab-item} macOS
:sync: macOS

An alternative option is to use the Apple Virtualization framework.

To switch the Multipass driver to the Apple Virtualization framework, run this command:

```{code-block} text
multipass set local.driver=applevz
```

From now on, all instances started with `multipass launch` will use the Apple Virtualization framework behind the scenes.

````

````{tab-item} Windows
:sync: Windows

You can change the hypervisor that Multipass uses to Hyper-V.

```{note}
The Hyper-V driver is deprecated as of Multipass version 1.17 (see
[Migrate from Hyper-V to the HCS driver on Windows](/how-to-guides/customise-multipass/migrate-from-hyperv-to-hcs-on-windows)).
```

To switch the Multipass driver to Hyper-V, run this command:

```{code-block} powershell
multipass set local.driver=hyperv
```

From then on, all instances started with multipass launch will use Hyper-V behind the scenes.

````

`````

## Switch back to the default driver

> See also: {ref}`reference-command-line-interface-stop`, {ref}`reference-settings-local-driver`

`````{tab-set}

````{tab-item} Linux
:sync: Linux

On Linux, there is only one driver and changing it is not possible.

````

````{tab-item} macOS
:sync: macOS

If you want to switch back to the default driver, run:

```{code-block} text
multipass set local.driver=qemu
```

Instances are tied to the driver they were created with; after switching, they won't be visible until you switch back.

````

````{tab-item} Windows
:sync: Windows

If you want to switch back to the default driver:

```{code-block} powershell
multipass set local.driver=hcs
```

Instances are tied to the driver they were created with; after switching, they won't be visible until you switch back.

````

`````
