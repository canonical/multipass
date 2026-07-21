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
Image                       Aliases              Version          Description
22.04                       jammy                20260515         Ubuntu 22.04 LTS
24.04                       noble                20260518         Ubuntu 24.04 LTS
25.10                       questing             20260520         Ubuntu 25.10
26.04                       resolute,lts,ubuntu  20260520         Ubuntu 26.04 LTS
core:core16                                      current          Ubuntu Core 16
core:core18                                      current          Ubuntu Core 18
core:core20                                      current          Ubuntu Core 20
core:core22                                      current          Ubuntu Core 22
core:core24                                      current          Ubuntu Core 24
core:core26                                      current          Ubuntu Core 26
debian                      trixie               20260601         Debian Trixie
fedora                                           20260422         Fedora 44
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
dancing-chipmunk        Running           192.168.64.8     Ubuntu 26.04 LTS
```

## Learn more about an instance

```
$ multipass info dancing-chipmunk

Name:           dancing-chipmunk
State:          Running
Snapshots:      0
IPv4:           192.168.64.8
Release:        Ubuntu 26.04 LTS
Image hash:     dced94c031cc (Ubuntu 26.04 LTS)
CPU(s):         1
Load:           5.70 4.58 2.63
Disk usage:     3.3GiB out of 4.8GiB
Memory usage:   769.0MiB out of 953.0MiB
Mounts:         --
```

## Connect to a running instance

```
$ multipass shell dancing-chipmunk

Welcome to Ubuntu 26.04 LTS (GNU/Linux 7.0.0-22-generic x86_64)
...
```

Don't forget to logout (or Ctrl-D) or you may find yourself heading all the way down Inception levels... ;)

## Run commands inside an instance from outside

```
$ multipass exec dancing-chipmunk -- lsb_release -a

No LSB modules are available.
Distributor ID: Ubuntu
Description:    Ubuntu 26.04 LTS
Release:        26.04
Codename:       resolute
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
dancing-chipmunk        Deleted           --               Ubuntu 26.04 LTS
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
Multipass is tested with **Qt 6.9.1**. Newer patch versions along the 6.9 track (e.g. 6.9.2) should be fine. Newer minor versions may work, but they may cause compatibility issues.

You may use your preferred package manager to install Multipass.
Note that only the official installers are supported.
See the [installation guide](https://canonical.com/multipass/docs/stable/how-to-guides/install-multipass/) for details.

For backend support and system requirements, refer to the
[Multipass driver documentation](https://canonical.com/multipass/docs/stable/explanation/driver/).

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

# Community-led integrations

## Multipass MCP Server

- [WangYihang/multipass-mcp](https://github.com/WangYihang/multipass-mcp)

## Terraform providers

- [todoroff/terraform-provider-multipass](https://github.com/todoroff/terraform-provider-multipass)
- [larstobi/terraform-provider-multipass](https://github.com/larstobi/terraform-provider-multipass)

## Visual Studio Code extensions

- [geoffreynyaga/multipass-run](https://github.com/geoffreynyaga/multipass-run)
- [levalleyjack/multipass-manager-vscode](https://github.com/levalleyjack/multipass-manager-vscode)

# Additional information

[Multipass documentation](https://canonical.com/multipass/docs)

[gha-image]: https://github.com/canonical/multipass/workflows/Linux/badge.svg?branch=main

[gha-url]: https://github.com/canonical/multipass/actions?query=branch%3Amain+workflow%3ALinux

[snap-image]: https://snapcraft.io/multipass/badge.svg

[snap-url]: https://snapcraft.io/multipass

[codecov-image]: https://codecov.io/gh/canonical/multipass/branch/main/graph/badge.svg

[codecov-url]: https://codecov.io/gh/canonical/multipass
