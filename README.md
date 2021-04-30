# What is Multipass?

Multipass is a lightweight VM manager for Linux, Windows and macOS. It's designed
for developers who want a fresh Ubuntu environment with a single command. It uses
KVM on Linux, Hyper-V on Windows and HyperKit on macOS to run the VM with minimal
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

```
sudo snap install multipass
```

For macOS, you can download the installers [from GitHub](https://github.com/canonical/multipass/releases) or [use Homebrew](https://github.com/Homebrew/brew):

```
# Note, this may require you to enter your password for some sudo operations during install
brew install --cask multipass
```

On Windows, download the installer [from GitHub](https://github.com/canonical/multipass/releases).

# Usage

## Find available images
```
$ multipass find
Image                       Aliases           Version          Description
core                        core16            20200213         Ubuntu Core 16
core18                                        20200210         Ubuntu Core 18
16.04                       xenial            20200721         Ubuntu 16.04 LTS
18.04                       bionic,lts        20200717         Ubuntu 18.04 LTS
20.04                       focal             20200720         Ubuntu 20.04 LTS
daily:20.10                 devel,groovy      20200721         Ubuntu 20.10
```

## Launch a fresh instance of the current Ubuntu LTS
```
$ multipass launch ubuntu
Launching dancing-chipmunk...
Downloading Ubuntu 18.04 LTS..........
Launched: dancing chipmunk
```

## Check out the running instances
```
$ multipass list
Name                    State             IPv4             Release
dancing-chipmunk        RUNNING           10.125.174.247   Ubuntu 18.04 LTS
live-naiad              RUNNING           10.125.174.243   Ubuntu 18.04 LTS
snapcraft-asciinema     STOPPED           --               Ubuntu Snapcraft builder for Core 18
```

## Learn more about the VM instance you just launched
```
$ multipass info dancing-chipmunk
Name:           dancing-chipmunk
State:          RUNNING
IPv4:           10.125.174.247
Release:        Ubuntu 18.04.1 LTS
Image hash:     19e9853d8267 (Ubuntu 18.04 LTS)
Load:           0.97 0.30 0.10
Disk usage:     1.1G out of 4.7G
Memory usage:   85.1M out of 985.4M
```

## Connect to a running instance

```
$ multipass shell dancing-chipmunk
Welcome to Ubuntu 18.04.1 LTS (GNU/Linux 4.15.0-42-generic x86_64)

...
```

Don't forget to logout (or Ctrl-D) or you may find yourself heading all the
way down the Inception levels... ;)

## Run commands inside an instance from outside

```
$ multipass exec dancing-chipmunk -- lsb_release -a
No LSB modules are available.
Distributor ID:  Ubuntu
Description:     Ubuntu 18.04.1 LTS
Release:         18.04
Codename:        bionic
```

## Stop an instance to save resources
```
$ multipass stop dancing-chipmunk
```

## Delete the instance
```
$ multipass delete dancing-chipmunk
```

It will now show up as deleted:
```$ multipass list
Name                    State             IPv4             Release
snapcraft-asciinema     STOPPED           --               Ubuntu Snapcraft builder for Core 18
dancing-chipmunk        DELETED           --               Not Available
```

And when you want to completely get rid of it:

```
$ multipass purge
```

## Get help
```

multipass help
multipass help <command>
```

# Get involved!

Here's a set of steps to build and run your own build of Multipass:

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
sudo apt install libgl1 libpng16-16 libqt5core5a libqt5gui5 \
    libqt5network5 libqt5widgets5 libxml2 libvirt0 dnsmasq-base \
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
<multipass>/build/bin/multipass.gui                # GUI client
```

# More information

See [the Multipass documentation](https://discourse.ubuntu.com/c/multipass/doc).

[gha-image]: https://github.com/canonical/multipass/workflows/Linux/badge.svg?branch=main
[gha-url]: https://github.com/canonical/multipass/actions?query=branch%3Amain+workflow%3ALinux
[snap-image]: https://snapcraft.io/multipass/badge.svg
[snap-url]: https://snapcraft.io/multipass
[codecov-image]: https://codecov.io/gh/canonical/multipass/branch/main/graph/badge.svg
[codecov-url]: https://codecov.io/gh/canonical/multipass
