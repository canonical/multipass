(explanation-platform)=
# Platform

> See also: [How to install Multipass](/how-to-guides/install-multipass), [Host](/explanation/host), [Driver](/explanation/driver)

In Multipass, **platform** refers to the host computer's operating system. This can be Windows, macOS, or Linux.

## Feature disparities

While we strive to offer a uniform interface across the board, not all features are available on all platforms and there are some behaviour differences:

| Feature | Only supported on... | Notes |
|--- | --- | --- |
| **Windows terminal integration** | <ul><li>Windows</li></ul> | This affects the setting [`client.apps.windows-terminal.profiles`](/reference/settings/client-apps-windows-terminal-profiles) |
| **Mounts** | <ul><li>Linux</li><li>macOS</li><li>Windows <em>(disabled by default)</em></li></ul> | On Windows, mounts can be enabled with the setting [`local.privileged-mounts`](/reference/settings/local-privileged-mounts). <br/>This affects the [`mount`](/reference/command-line-interface/mount), [`umount`](/reference/command-line-interface/umount), and [`launch`](/reference/command-line-interface/launch) commands.|
| **Global IPv6 (QEMU)** | <ul><li>Linux</li><li>macOS</li></ul> | When using the QEMU driver, global IPv6 addresses are only available on macOS. |
| **Drivers** | <ul><li>Linux</li><li>macOS</li><li>Windows</li></ul> | Different drivers are available on different platforms. <br/>This affects the [`local.driver`](/reference/settings/local-driver) setting. <br/>See {ref}`driver-feature-disparities` for further behaviour differences depending on the selected driver. |
| **Bridging Wi-Fi networks** | <ul><li>macOS</li></ul> | Wi-Fi networks are not shown in the output of the [`networks`](/reference/command-line-interface/networks) command on Linux and Windows. |

<!--
- *Windows terminal integration* is offered only on Windows. This affects the setting [`client.apps.windows-terminal.profiles`](/reference/settings/client-apps-windows-terminal-profiles)
- *File and URL launches* are only possible on Linux. This affects the [`launch`](/reference/command-line-interface/launch) command.
- *Mounts* are disabled on Windows by default, though they can be enabled with the setting [`local.privileged-mounts`](/reference/settings/local-privileged-mounts). They are enabled by default on macOS and Linux. This affects the [`mount`](/reference/command-line-interface/mount), [`umount`](/reference/command-line-interface/umount), and [`launch`](/reference/command-line-interface/launch) commands.
- When using the QEMU driver, *extra networks* are supported only on macOS. This affects the [`networks`](/reference/command-line-interface/networks) command, as well as `--network` and `--bridged` options in [`launch`](/reference/command-line-interface/launch).
- Different *drivers* are available on different platforms. This affects the [`local.driver`](/reference/settings/local-driver) setting. See [driver](/explanation/driver) for further behaviour differences depending on what driver is selected.
- *Bridging Wi-Fi networks* is only supported at the moment on macOS. Thus, Wi-Fi networks are not shown in the output of the [`networks`](/reference/command-line-interface/networks) command on Linux and Windows.
-->
