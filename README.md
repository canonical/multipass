# What is Multipass?

Multipass is a lightweight VM manager for Linux, Windows and macOS. It's designed for developers who want to spin up a
fresh Ubuntu environment with a single command. It uses KVM on Linux, Hyper-V on Windows and QEMU on macOS to run
virtual machines with minimal overhead. It can also use VirtualBox on Windows and macOS. Multipass will fetch Ubuntu
images for you and keep them up to date.

Multipass lets developers quickly create and manage Ubuntu virtual machines on Linux, Windows, or macOS. It’s ideal for testing, development, or learning Linux environments without setting up a full virtual machine manually.

Since it supports metadata for cloud-init, you can simulate a small cloud deployment on your laptop or workstation.

## Project status

| Service                                              | Status                                          |
|------------------------------------------------------|:------------------------------------------------|
| [CI](https://github.com/canonical/multipass/actions) | [![Build Status][gha-image]][gha-url]           |
| [Snap](https://snapcraft.io/)                        | [![Build Status][snap-image]][snap-url]         |
| [Codecov](https://codecov.io/)                       | [![Codecov Status][codecov-image]][codecov-url] |

# Installation

* On **Linux**, Multipass is available as a snap:

  ```
  sudo snap install multipass
  ```

* On **macOS**, download the installer [from GitHub](https://github.com/canonical/multipass/releases).

  Alternatively, you can use [Homebrew](https://github.com/Homebrew/brew). Please note that this method is **not
  officially supported**, as it is not maintained by the Multipass team, but by the community. Multipass is available as
  a cask:

  ```
  brew install --cask multipass
  ```

  Please note that you may be required to enter your password for some sudo operations during installation. You may also
  need to disable the firewall to launch a multipass instance successfully on macOS.

* On **Windows**, download the installer [from GitHub](https://github.com/canonical/multipass/releases).

For more information, see [How to install Multipass](https://canonical.com/multipass/docs/install-multipass).

# Usage

Here are some pointers to get started with Multipass. For a more comprehensive learning experience, please check out the
Multipass [Tutorial](https://canonical.com/multipass/docs/tutorial).

## Find available images

```
$ multipass find

Image                       Aliases           Version          Description
20.04                       focal             20240731         Ubuntu 20.04 LTS
22.04                       jammy             20240808         Ubuntu 22.04 LTS
24.04                       noble,lts         20240806         Ubuntu 24.04 LTS

Blueprint                   Aliases           Version          Description
anbox-cloud-appliance                         latest           Anbox Cloud Appliance
charm-dev                                     latest           A development and testing environment for charmers
docker                                        0.4              A Docker environment with Portainer and related tools
jellyfin                                      latest           Jellyfin is a Free Software Media System that puts you in control of managing and streaming your media.
minikube                                      latest           minikube is local Kubernetes
ros-noetic                                    0.1              A development and testing environment for ROS Noetic.
ros2-humble                                   0.1              A development and testing environment for ROS 2 Humble.
```

## Launch a fresh instance of the current Ubuntu LTS

```
$ multipass launch lts

Launched: dancing-chipmunk
```

## Check out the running instances

```
$ multipass list

Name                    State             IPv4             Image
dancing-chipmunk        Running           192.168.64.8     Ubuntu 24.04 LTS
phlegmatic-bluebird     Stopped           --               Ubuntu 22.04 LTS
docker                  Running           192.168.64.11    Ubuntu 22.04 LTS
                                          172.17.0.1
```

## Learn more about an instance

```
$ multipass info dancing-chipmunk

Name:           dancing-chipmunk
State:          Running
Snapshots:      0
IPv4:           192.168.64.8
Release:        Ubuntu 24.04 LTS
Image hash:     e2608bfdbc44 (Ubuntu 24.04 LTS)
CPU(s):         1
Load:           5.70 4.58 2.63
Disk usage:     3.3GiB out of 4.8GiB
Memory usage:   769.0MiB out of 953.0MiB
Mounts:         --
```

## Connect to a running instance

```
$ multipass shell dancing-chipmunk

Welcome to Ubuntu 24.04 LTS (GNU/Linux 6.8.0-39-generic aarch64)
...
```

Don't forget to logout (or Ctrl-D) or you may find yourself heading all the way down Inception levels... ;)

## Run commands inside an instance from outside

```
$ multipass exec dancing-chipmunk -- lsb_release -a

No LSB modules are available.
Distributor ID:  Ubuntu
Description:     Ubuntu 24.04 LTS
Release:         24.04
Codename:        noble
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

```
$ multipass list

Name                    State             IPv4             Image
dancing-chipmunk        Deleted           --               Ubuntu 24.04 LTS
phlegmatic-bluebird     Stopped           --               Ubuntu 22.04 LTS
docker                  Running           192.168.64.11    Ubuntu 22.04 LTS
                                          172.17.0.1
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

# Contributing

The Multipass team appreciates contributions to the project, through pull requests, issues, or discussions and questions
on the [Discourse forum](https://discourse.ubuntu.com/c/multipass/21). Please read the policy sections below carefully
before contributing to the project. <!-- TODO: link to guidelines -->

## Building Multipass

Please follow the platform-specific build instructions in the files below:

* [BUILD.linux.md](./BUILD.linux.md) for Linux
* [BUILD.macOS.md](./BUILD.macOS.md) for macOS
* [BUILD.windows.md](./BUILD.windows.md) for Windows

### Generic build tips

**Qt version compatibility**
Multipass is tested with **Qt 6.2.4**. Newer patch versions along the 6.2 track (e.g. 6.2.8) should be fine. Newer minor versions may work, but they may cause compatibility issues.

**ARM64 and cross-compilation**
If you're using Apple Silicon (arm64) or cross-compiling, ensure that your `PATH` and `CMAKE_PREFIX_PATH` environment variables point to the correct Qt installation for your system architecture.

You may use your preferred package manager to install Multipass.
Note that only the official installers are supported.
See the [installation guide](https://documentation.ubuntu.com/multipass/en/latest/how-to-guides/install-multipass/) for details.

For backend support and system requirements, refer to the
[Multipass driver documentation](https://documentation.ubuntu.com/multipass/en/latest/explanation/driver/).

If you notice outdated information or inconsistencies in these files, please [open an issue](https://github.com/canonical/multipass/issues) or, even better, submit a pull request!

You can also reference our [GitHub Actions CI](https://github.com/canonical/multipass/actions) to see how Multipass is built and tested across platforms.

### Automatic linker selection

***Requires (>= CMake 3.29)***

To accelerate the build, the build system will attempt to locate and utilize `mold` or `lld` (respectively) in place of
the default linker of the toolchain. To override, set
[CMAKE_LINKER_TYPE](https://cmake.org/cmake/help/latest/variable/CMAKE_LINKER_TYPE.html#cmake-linker-type) at CMake
configure step.

## Code of Conduct

When contributing, you must adhere to the [Code of Conduct](https://ubuntu.com/community/ethos/code-of-conduct).

## Copyright

The code in this repository is licensed under GNU General Public License v3.0.
See [LICENSE](https://github.com/canonical/multipass/blob/main/LICENSE) for more information.

## License agreement

All contributors must sign the [Canonical contributor license agreement (CLA)](https://ubuntu.com/legal/contributors),
which gives Canonical permission to use the contributions. Without the CLA, contributions cannot be accepted.

## Pull requests

Changes to this project should be proposed as pull requests. Proposed changes will then go through review and once
approved, be merged into the main branch.

# Additional information

[Multipass documentation](https://canonical.com/multipass/docs)

[gha-image]: https://github.com/canonical/multipass/workflows/Linux/badge.svg?branch=main

[gha-url]: https://github.com/canonical/multipass/actions?query=branch%3Amain+workflow%3ALinux

[snap-image]: https://snapcraft.io/multipass/badge.svg

[snap-url]: https://snapcraft.io/multipass

[codecov-image]: https://codecov.io/gh/canonical/multipass/branch/main/graph/badge.svg

[codecov-url]: https://codecov.io/gh/canonical/multipass
