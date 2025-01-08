(how-to-guides-install-multipass)=
# Install Multipass

<!-- This page combines the 3 different how-to pages ([Linux](/), [macOS](/), [Windows](/)) into a single page, using conditional tabs. -->

<!-- Merged contributions by @bagustris on the Open Documentation Academy -->

This guide explains how to install and manage Multipass on your system.

```{note}
Select the tab corresponding to your operating system (e.g. Linux) to display the relevant content in each section. Your decision will stick until you select another OS from the drop-down menu.
```

## Check prerequisites

[tabs]

[tab version="Linux"]

Multipass for Linux is published as a [snap package](https://snapcraft.io/docs/), available on the [Snap Store](https://snapcraft.io/multipass). Before you can use it, you need to [install `snapd`](https://docs.snapcraft.io/core/install). `snapd` is included in Ubuntu by default.

[/tab]

[tab version="macOS"]

<!--### Hypervisor.framework / hyperkit-->

The default backend on macOS is `qemu`, wrapping Apple's Hypervisor framework. You can use any Mac (M1, M2, or Intel based) with **macOS 10.15 Catalina or later** installed.

[/tab]

[tab version="Windows"]

### Hyper-V

Only **Windows 10 Pro** or **Enterprise** version **1803** ("April 2018 Update") **or later** are currently supported, due to the necessary version of Hyper-V only being available on those versions.

### VirtualBox

Multipass also supports using VirtualBox as a virtualization provider.  You can download the latest version from the [VirtualBox download page](https://www.oracle.com/technetwork/server-storage/virtualbox/downloads/index.html).

[/tab]

[/tabs]

## Install

[tabs]

[tab version="Linux"]

To install Multipass, run the following command:

```plain
snap install multipass
```

You can also use the `edge` channel to get the latest development build:

```plain
snap install multipass --edge
```

Make sure you're part of the group that Multipass gives write access to its socket (`sudo` in this case, but it may also be `wheel` or `admin`, depending on your distribution).

1. Run this command to check which group is used by the Multipass socket:
    ```plain
    ls -l /var/snap/multipass/common/multipass_socket
    ```
    The output will be similar to the following:
    ```plain
    srw-rw---- 1 root sudo 0 Dec 19 09:47 /var/snap/multipass/common/multipass_socket
    ```

2. Run the  `groups` command to make sure you are a member of that group (in our example, "sudo"):

    ```plain
    groups | grep sudo
    ```
    The output will be similar to the following:
    ```plain
    adm cdrom sudo dip plugdev lpadmin
    ```

You can view more information on the snap package using the `snap info` command:

```plain
snap info multipass
```
For example:
```plain
name:      multipass
summary:   Instant Ubuntu VMs
publisher: Canonical✓
store-url: https://snapcraft.io/multipass
contact:   https://github.com/CanonicalLtd/multipass/issues/new
license:   GPL-3.0
description: |
  Multipass is a tool to launch and manage VMs on Windows, Mac and Linux that simulates a cloud
  environment with support for cloud-init. Get Ubuntu on-demand with clean integration to your IDE
  and version control on your native platform.
  ...
commands:
  - multipass.gui
  - multipass
services:
  multipass.multipassd: simple, enabled, active
snap-id:      mA11087v6dR3IEcQLgICQVjuvhUUBUKM
tracking:     latest/candidate
refresh-date: 5 days ago, at 10:13 CEST
channels:
  latest/stable:    1.3.0                 2020-06-17 (2205) 228MB -
  latest/candidate: 1.3.0                 2020-06-17 (2205) 228MB -
  latest/beta:      1.3.0-dev.17+gf89e1db 2020-04-28 (2019) 214MB -
  latest/edge:      1.4.0-dev.83+g149f10a 2020-06-17 (2216) 228MB -
installed:          1.3.0                            (2205) 228MB -
```

[/tab]

[tab version="macOS"]

```{note}
You will need an account with administrator privileges to complete the installation.
```

Download the latest installer from [our download page](https://canonical.com/multipass/download/macos). You can also get pre-release versions from the [GitHub releases](https://github.com/canonical/multipass/releases/) page, look for the `.pkg` package.

Run the downloaded installer and follow the guided procedure. 

![Multipass installer on macOS|658x475](upload://oBqM17GtMd6dzpBJ2OWl3fexIP2.png)

[/tab]

[tab version="Windows"]

```{note}
You will need either Hyper-V enabled (only Windows 10 Professional or Enterprise), or VirtualBox installed. See [Check prerequisites](#check-prerequisites).
```

Download the latest installer from [our download page](https://canonical.com/multipass/download/windows). You can also get pre-release versions from the [GitHub releases](https://github.com/CanonicalLtd/multipass/releases/) page, look for the `.msi` file. 

Run the downloaded installer and follow the guided procedure. The installer will require to be granted Administrator privileges.

[/tab]

Alternatively, you can also check your preferred package manager to see if it provides Multipass, although this option is not officially supported.

[/tabs]

## Run

[tabs]

[tab version="Linux"]

You've installed Multipass. Time to run your first commands! Use `multipass version` to check your version or `multipass launch` to create your first instance.

[/tab]

[tab version="macOS"]

You've installed Multipass. Time to run your first commands! Use `multipass version` to check your version or `multipass launch` to create your first instance.

> See also: [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver), [How to use a different terminal from the system icon](/how-to-guides/customise-multipass/use-a-different-terminal-from-the-system-icon)

[/tab]

[tab version="Windows"]

You've installed Multipass. Time to run your first commands! Launch a **Command Prompt** (`cmd.exe`) or **PowerShell** as a regular user.  Use `multipass version` to check your version or `multipass launch` to create your first instance.

Multipass defaults to using Hyper-V as its virtualization provider.  If you'd like to use VirtualBox, you can do so using the following command:

```plain
multipass set local.driver=virtualbox
```

> See also: [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver).

[/tab]

[/tabs]

## Upgrade

[tabs]

[tab version="Linux"]

As the installation happened via snap, you don't need to worry about upgrading---it will be done automatically.

[/tab]

[tab version="macOS"]

```{note}
You will need an account with administrator privileges to complete the upgrade.
```

To upgrade, download the latest installer from [our download page](https://canonical.com/multipass/download/macos). You can also get pre-release versions from the [GitHub releases](https://github.com/canonical/multipass/releases/) page, look for the `.pkg` package.

Run the downloaded installer and follow the guided procedure. 

Any existing instances will be preserved.

[/tab]

[tab version="Windows"]

To upgrade, [download the latest installer](https://canonical.com/multipass/download/windows) and run it. You can also get pre-release versions from the [GitHub releases](https://github.com/canonical/multipass/releases/) page, look for the `.msi` package.

You will be asked to uninstall the old version, and then whether to remove all data when uninstalling. If you want to keep your existing instances, answer "No" (default).
[/tab]

[/tabs]

## Uninstall

[tabs]

[tab version="Linux"]
To uninstall Multipass, run the following command:

```plain
snap remove multipass
```
[/tab]

[tab version="macOS"]

To uninstall Multipass, run the script:
```plain
sudo sh "/Library/Application Support/com.canonical.multipass/uninstall.sh"
```

[/tab]

[tab version="Windows"]
Uninstall Multipass as you would any other program, following the usual procedure.
[/tab]

[/tabs]

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/install-multipass" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

---

<small>**Contributors:** @saviq , @townsend , @sowasred2012 , @ya-bo-ng , @shuuji3 , @henryschreineriii , @sven , @nick3499 , @undefynd , @sparkiegeek , @nottrobin , @tmihoc , @nhart , @luisp , @sharder996 , @aaryan-porwal , @andreitoterman , @ricab , @gzanchi , @bagustris </small>

