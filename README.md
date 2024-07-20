# What is Multipass?

Multipass is a lightweight VM manager for Linux, Windows and macOS. It's designed
for developers who want a fresh Ubuntu environment with a single command. It uses
KVM on Linux, Hyper-V on Windows and QEMU on macOS to run the VM with minimal
overhead. It can also use VirtualBox on Windows and macOS.
Multipass will fetch images for you and keep them up to date.

Since it supports metadata for cloud-init, you can simulate a small cloud
deployment on your laptop or workstation.

## Project Status

| Service | Status |
|-----|:---|
| [CI](https://github.com/canonical/multipass/actions) |  [![Build Status][gha-image]][gha-url]  |
| [Snap](https://snapcraft.io/) |  [![Build Status][snap-image]][snap-url]  |
| [Codecov](https://codecov.io/) |  [![Codecov Status][codecov-image]][codecov-url]  |

# Install Multipass

On Linux it's available as a snap:

```shell
sudo snap install multipass
```

For macOS, you can download the installers [from GitHub](https://github.com/canonical/multipass/releases) or [use Homebrew](https://github.com/Homebrew/brew):

```shell
# Note, this may require you to enter your password for some sudo operations during install
# Mac OS users may need to disable their firewall to launch a multipass instance successfully
brew install --cask multipass
```

On Windows, download the installer [from GitHub](https://github.com/canonical/multipass/releases).

# Usage

## Find available images
```shell
$ multipass find
Image                       Aliases           Version          Description
core                        core16            20200818         Ubuntu Core 16
core18                                        20211124         Ubuntu Core 18
core20                                        20230119         Ubuntu Core 20
core22                                        20230717         Ubuntu Core 22
20.04                       focal             20240710         Ubuntu 20.04 LTS
22.04                       jammy             20240716         Ubuntu 22.04 LTS
24.04                       noble,lts         20240710         Ubuntu 24.04 LTS
daily:24.10                 oracular,devel    20240719         Ubuntu 24.10
appliance:adguard-home                        20200812         Ubuntu AdGuard Home Appliance
appliance:mosquitto                           20200812         Ubuntu Mosquitto Appliance
appliance:nextcloud                           20200812         Ubuntu Nextcloud Appliance
appliance:openhab                             20200812         Ubuntu openHAB Home Appliance
appliance:plexmediaserver                     20200812         Ubuntu Plex Media Server Appliance
```

## Launch a fresh instance of the current Ubuntu LTS
```shell
$ multipass launch --name vm-01 lts
Launched: vm-01
```

## Check out the running instances
```shell
$ multipass list
Name                    State             IPv4             Image
vm-01                   Running           10.191.202.10    Ubuntu 24.04 LTS
```

## Learn more about the VM instance you just launched
```shell
$ multipass info vm-01
Name:           vm-01
State:          Running
Snapshots:      0
IPv4:           10.191.202.10
Release:        Ubuntu 24.04 LTS
Image hash:     5d482ff93a2b (Ubuntu 24.04 LTS)
CPU(s):         1
Load:           0.30 0.26 0.10
Disk usage:     1.7GiB out of 4.8GiB
Memory usage:   285.4MiB out of 956.1MiB
Mounts:         --
```

## Connect to a running instance

```shell
$ multipass shell vm-01
Welcome to Ubuntu 24.04 LTS (GNU/Linux 6.8.0-36-generic x86_64)
...
ubuntu@vm-01:~$
```

Don't forget to logout (or Ctrl-D) or you may find yourself heading all the
way down the Inception levels... ;)

## Run commands inside an instance from outside

```shell
$ multipass exec vm-01 -- lsb_release -a
No LSB modules are available.
Distributor ID: Ubuntu
Description:    Ubuntu 24.04 LTS
Release:        24.04
Codename:       noble
```

## Stop an instance to save resources
```shell
$ multipass stop vm-01
```

## Delete the instance
```shell
$ multipass delete vm-01
```

It will now show up as deleted:
```shell
$ multipass list
Name                    State             IPv4             Image
vm-01                   Deleted           --               Ubuntu 24.04 LTS
```

And when you want to completely get rid of it:

```shell
$ multipass purge
```

## Get help
```shell
$ multipass help
$ multipass help <command>
```

# Get involved!

Here's a set of steps to build and run your own build of Multipass. Please note that the following instructions are for building Multipass for Linux only. These instructions do not support building packages for macOS or Windows systems.

## Build Dependencies

```
cd <multipass>
apt install devscripts equivs
mk-build-deps -s sudo -i
```

## Building

```
cd <multipass>
git submodule update --init --recursive
mkdir build
cd build
cmake ../
make
```

## Running Multipass daemon and client

First, install multipass's runtime dependencies. On amd64 architecture, you can achieve that with:

```
sudo apt update
sudo apt install libgl1 libpng16-16 libqt6core6 libqt6gui6 \
    libqt6network6 libqt6widgets6 libxml2 libvirt0 dnsmasq-base \
    dnsmasq-utils qemu-system-x86 qemu-utils libslang2 iproute2 \
    iptables iputils-ping libatm1 libxtables12 xterm
```

Then run multipass's daemon:
```
sudo <multipass>/build/bin/multipassd &
```

Copy the desktop file multipass clients expect to find in your home:

```
mkdir -p ~/.local/share/multipass/
cp <multipass>/data/multipass.gui.autostart.desktop ~/.local/share/multipass/
```

Optionally, enable auto-complete in bash:

```
source <multipass>/completions/bash/multipass
```

Finally, use multipass's clients:

```
<multipass>/build/bin/multipass launch --name foo  # CLI client
<multipass>/build/bin/multipass.gui        # GUI client
```

# More information

See [the Multipass documentation](https://discourse.ubuntu.com/c/multipass/doc).

[gha-image]: https://github.com/canonical/multipass/workflows/Linux/badge.svg?branch=main
[gha-url]: https://github.com/canonical/multipass/actions?query=branch%3Amain+workflow%3ALinux
[snap-image]: https://snapcraft.io/multipass/badge.svg
[snap-url]: https://snapcraft.io/multipass
[codecov-image]: https://codecov.io/gh/canonical/multipass/branch/main/graph/badge.svg
[codecov-url]: https://codecov.io/gh/canonical/multipass
