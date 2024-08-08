# What is Multipass?

Multipass is a lightweight VM manager for Linux, Windows and macOS. It's designed for developers who want to spin up a fresh Ubuntu environment with a single command. It uses KVM on Linux, Hyper-V on Windows and QEMU on macOS to run virtual machines with minimal overhead. It can also use VirtualBox on Windows and macOS. Multipass will fetch Ubuntu images for you and keep them up to date.

Since it supports metadata for cloud-init, you can simulate a small cloud deployment on your laptop or workstation.

## Project status

| Service | Status |
|-----|:---|
| [CI](https://github.com/canonical/multipass/actions) |  [![Build Status][gha-image]][gha-url]  |
| [Snap](https://snapcraft.io/) |  [![Build Status][snap-image]][snap-url]  |
| [Codecov](https://codecov.io/) |  [![Codecov Status][codecov-image]][codecov-url]  |

# Installation

* On Linux, Multipass is available as a snap:

  ```
  sudo snap install multipass
  ```

* On macOS, you can download the installer [from GitHub](https://github.com/canonical/multipass/releases) or use [Homebrew](https://github.com/Homebrew/brew):

  ```
  brew install --cask multipass
  ```
  
  Please note that you may be required to enter your password for some sudo operations during installation. You may also need to disable the firewall to launch a multipass instance successfully on macOS.

* On Windows, download the installer [from GitHub](https://github.com/canonical/multipass/releases).

For more information, see [How to install Multipass](https://multipass.run/docs/install-multipass).

# Usage

Here are some pointers to get started with Multipass. For a more comprehensive learning experience, please check out the Multipass [Tutorial](https://multipass.run/docs/tutorial).

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

## Learn more about an instance

```
$ multipass info dancing-chipmunk

Name:           dancing-chipmunk
State:          RUNNING
IPv4:           10.125.174.247
Release:        Ubuntu 18.04.1 LTS
Image hash:     19e9853d8267 (Ubuntu 18.04 LTS)
CPU(s):         1
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

Don't forget to logout (or Ctrl-D) or you may find yourself heading all the way down Inception levels... ;)

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

## Delete an instance

```
$ multipass delete dancing-chipmunk
```

The instance will now show up as deleted:

```$ multipass list

Name                    State             IPv4             Release
snapcraft-asciinema     STOPPED           --               Ubuntu Snapcraft builder for Core 18
dancing-chipmunk        DELETED           --               Not Available
```

If you want to completely get rid of it:

```
$ multipass purge
```

## Get help

```
multipass help
multipass help <command>
```

# Make your own build

Here's a set of steps to make and run your own build of Multipass. 

Please note that the following instructions are for building Multipass for Linux only. These instructions do not support building packages for macOS or Windows systems.

## Build dependencies

```
cd <multipass>
apt install devscripts equivs
mk-build-deps -s sudo -i
```

## Build Multipass

```
cd <multipass>
git submodule update --init --recursive
mkdir build
cd build
cmake ../
make
```

Please note that if you're working on a forked repository that you created using the "Copy the main branch only" option, the repository will not include the necessary git tags to determine the Multipass version during CMake configuration. In this case, you need to manually fetch the tags from the upstream by running `git fetch --tags https://github.com/canonical/multipass.git` in the `<multipass>` source code directory. 

## Run the Multipass daemon and client

First, install Multipass's runtime dependencies. On AMD64 architecture, you can do this with:

```
sudo apt update
sudo apt install libgl1 libpng16-16 libqt6core6 libqt6gui6 \
    libqt6network6 libqt6widgets6 libxml2 libvirt0 dnsmasq-base \
    dnsmasq-utils qemu-system-x86 qemu-utils libslang2 iproute2 \
    iptables iputils-ping libatm1 libxtables12 xterm
```

Then run the Multipass daemon:

```
sudo <multipass>/build/bin/multipassd &
```

Copy the desktop file that Multipass clients expect to find in your home:

```
mkdir -p ~/.local/share/multipass/
cp <multipass>/data/multipass.gui.autostart.desktop ~/.local/share/multipass/
```

Optionally, enable auto-complete in Bash:

```
source <multipass>/completions/bash/multipass
```

Finally, launch the Multipass clients:

```
<multipass>/build/bin/multipass launch --name foo  # CLI client
<multipass>/build/bin/multipass.gui                # GUI client
```

# Contributing guidelines

The Multipass team appreciates contributions to the project, through pull requests, issues, or discussions and questions on the [Discourse forum](https://discourse.ubuntu.com/c/multipass/21).

Please read the following guidelines carefully before contributing to the project.

## Code of Conduct

When contributing, you must adhere to the [Code of Conduct](https://ubuntu.com/community/ethos/code-of-conduct).

## Copyright

The code in this repository is licensed under GNU General Public License v3.0. See [LICENSE](https://github.com/canonical/multipass/blob/main/LICENSE) for more information.

## License agreement

All contributors must sign the [Canonical contributor license agreement (CLA)](https://ubuntu.com/legal/contributors), which gives Canonical permission to use the contributions. Without the CLA, contributions cannot be accepted.

## Pull requests

Changes to this project should be proposed as pull requests. Proposed changes will then go through review and once approved, be merged into the main branch.

# Additional information

[Multipass documentation](https://multipass.run/docs)

[gha-image]: https://github.com/canonical/multipass/workflows/Linux/badge.svg?branch=main
[gha-url]: https://github.com/canonical/multipass/actions?query=branch%3Amain+workflow%3ALinux
[snap-image]: https://snapcraft.io/multipass/badge.svg
[snap-url]: https://snapcraft.io/multipass
[codecov-image]: https://codecov.io/gh/canonical/multipass/branch/main/graph/badge.svg
[codecov-url]: https://codecov.io/gh/canonical/multipass
