(explanation-driver)=
# Driver

```{warning}
Support for LXD will be deprecated and removed in a future release.
```

> See also: [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver), [`local.driver`](/reference/settings/local-driver), [Instance](/explanation/instance), [Platform](/explanation/platform)

A **driver** is the technology through which Multipass emulates a running machine. It corresponds to a hypervisor or intermediary technology to run virtual machines. The driver is sometimes also referred to as "backend".

Multipass relies on a driver to operate. It supports multiple drivers, but it runs with a single driver at a time. There is a [Multipass setting](/reference/settings/index) to select the driver: [`local.driver`](/reference/settings/local-driver).

On some platforms, it is possible to select a driver during installation. Until it is manually set, a platform-appropriate default driver is used.

## Supported drivers

Different sets of drivers are available on different platforms:

- On Linux, Multipass can be configured to use QEMU and LXD. As of Multipass version 1.16, LXD is deprecated.
- On macOS, the options are QEMU and VirtualBox. As of Multipass version 1.13, Hyperkit is no longer available.
- On Windows, Multipass uses Hyper-V (only available on Windows Pro) or VirtualBox.

## Default drivers

When Multipass is installed, the following drivers are selected by default:

- On Linux and macOS, QEMU is used.
- On Windows, the default driver depends on the OS version:
  + Hyper-V on Windows Pro
  + VirtualBox on Windows Home

## Instance scopes

In general, Multipass instances are tied to a single driver, with the {ref}`explanation-driver-exceptions` listed below. The set of instances that were launched with one driver are available only while that driver is in use.

When a new driver is selected, Multipass switches to a separate instance scope. There, the set of existing instances is empty to begin with. Users can launch instances with the same name in different drivers and changes to instances with one driver have no effect on the instances of another.

Nonetheless, instances are preserved across drivers. After switching back to a previously-used driver, Multipass restores the corresponding instance scope. It attempts to restore the state instances were in just before the switch and users can interact with them just as before.

(explanation-driver-exceptions)=
### Exceptions

There are two exceptions to the above:

  <!-- - On Linux, QEMU and libvirt (deprecated) share the same driver scope. -->
  - On macOS, stopped Hyperkit instances are automatically migrated to QEMU by Multipass's version 1.12 or later (see [How to migrate from Hyperkit to QEMU on macOS](/how-to-guides/customise-multipass/migrate-from-hyperkit-to-qemu-on-macos)).

(driver-feature-disparities)=
## Feature disparities

While we strive to offer a uniform interface across the board, not all features are available on all backends and there are some behaviour differences:

| Feature | Only supported on... | Notes |
|--- | --- | --- |
| **Native mounts** | <ul><li>Hyper-V</li><li>LXD (deprecated)</li><li>QEMU</li></ul> | This affects the `--type` option in the [`mount`](/reference/command-line-interface/mount) command). |
| **Extra networks** | <ul><li>Hyper-V</li><li>LXD (deprecated)</li><li>QEMU</li><li>VirtualBox</li></ul> | This affects the [`networks`](/reference/command-line-interface/networks) command, as well as the `--network` and `--bridged` options in [`launch`](/reference/command-line-interface/launch). |
| **Snapshots** | <ul><li>Hyper-V</li><li>QEMU</li><li>VirtualBox</li></ul> |  |
| **Clone** | <ul><li>Hyper-V</li><li>QEMU</li><li>VirtualBox</li></ul> |  This affects the [`clone`](/reference/command-line-interface/clone) command.|
| **VM suspension** | <ul><li>Hyper-V</li><li>QEMU</li><li>VirtualBox</li></ul> | This affects the [`suspend`](/reference/command-line-interface/suspend) command. |

<!-- old formatting
- **Native mounts** are supported only on Hyper-V, QEMU, and LXD. This affects the `--type` option in the [`mount`](/reference/command-line-interface/mount) command).
- **Extra networks** are supported only on LXD, Hyper-V, VirtualBox, and QEMU on macOS. This affects the [`networks`](/reference/command-line-interface/networks) command, as well as the `--network` and `--bridged` options in [`launch`](/reference/command-line-interface/launch).
- **Snapshots** are supported only on QEMU, Hyper-V, and VirtualBox *[the latter since version 1.15]*.
- **VM suspension** is supported on QEMU, libvirt, Hyper-V, and VirtualBox. This affects the [`suspend`](/reference/command-line-interface/suspend) command.
-->

```{note}
There are also feature disparities depending on the host platform. See [Platform](/explanation/platform) for more details.
```
