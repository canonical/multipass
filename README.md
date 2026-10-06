# What is Multipass?

Multipass is a lightweight VM manager for Linux, Windows and macOS. It's designed for developers who want to spin up a
fresh Ubuntu environment with a single command. It uses KVM on Linux, Hyper-V on Windows and QEMU on macOS to run
virtual machines with minimal overhead. It can also use VirtualBox on Windows and macOS. Multipass will fetch Ubuntu
images for you and keep them up to date.

Since it supports metadata for cloud-init, you can simulate a small cloud deployment on your laptop or workstation.

## Project status

| Service                                              | Status                                          |
|------------------------------------------------------|:------------------------------------------------|
| [CI](https://github.com/canonical/multipass/actions) | [![Linux CI][gha-image-linux]][gha-url-linux] [![Windows CI][gha-image-windows]][gha-url-windows] [![macOS CI][gha-image-macos]][gha-url-macos] |
| [Snap](https://snapcraft.io/)                        | [![Build Status][snap-image]][snap-url]         |
| [Codecov](https://codecov.io/)                       | [![Codecov Status][codecov-image]][codecov-url] |

# Installation

For more information, see [How to install Multipass](https://canonical.com/multipass/docs/stable/how-to-guides/install-multipass/).

### Linux

Multipass is available as a snap:

  ```
  sudo snap install multipass
  ```

### macOS

Download the installer [from GitHub](https://github.com/canonical/multipass/releases).

  Alternatively, you can use [Homebrew](https://github.com/Homebrew/brew) which is **not
  officially supported**, as it is not maintained by the Multipass team, but by the community. Multipass is available
  as a cask:

  ```
  brew install --cask multipass
  ```

  Please note that you may be required to enter your password for some sudo operations during installation. You may also
  need to disable the firewall to launch a multipass instance successfully on macOS.

### Windows

Download the installer [from GitHub](https://github.com/canonical/multipass/releases).

# Usage

Here are some pointers to get started with Multipass.


| Task | Command |
|----|----|
| Find available images | `multipass find` |
| Launch an instance with the current Ubuntu LTS | `multipass launch lts` |
| List existing instances | `multipass list` |
| Get info about an instance | `multipass info <instance-name>` |
| Connect to a running instance | `multipass shell <instance-name>` |
| Run a command inside an instance | `multipass exec <instance-name> -- <command>` |
| Stop an instance | `multipass stop <instance-name>` |
| Delete an instance | `multipass delete <instance-name>` <br/> `multipass purge` |
| Get help | `multipass help` <br/> `multipass help <command>` |

For a more comprehensive learning experience, please check out the
[Multipass Tutorial](https://canonical.com/multipass/docs/stable/tutorial/) and consult the [Multipass documentation](https://canonical.com/multipass/docs).

# Building and Contributing

## Building Multipass

Please follow the platform-specific build instructions in the files below:

* [BUILD.linux.md](./BUILD.linux.md) for Linux
* [BUILD.macOS.md](./BUILD.macOS.md) for macOS
* [BUILD.windows.md](./BUILD.windows.md) for Windows

### Generic build tips

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

## Contributing

The Multipass team appreciates contributions to the project, through pull requests, issues, or discussions.
Changes to this project should be proposed as pull requests. Proposed changes will then go through review and once
approved, be merged into the main branch.

Before contributing, please read the [Contributing document](CONTRIBUTING.md) carefully and follow the [Contributing Guidelines](GUIDELINES.md).

# Community-led integrations

### Multipass MCP Server

- [WangYihang/multipass-mcp](https://github.com/WangYihang/multipass-mcp)

### Terraform providers

- [todoroff/terraform-provider-multipass](https://github.com/todoroff/terraform-provider-multipass)
- [larstobi/terraform-provider-multipass](https://github.com/larstobi/terraform-provider-multipass)

### Visual Studio Code extensions

- [geoffreynyaga/multipass-run](https://github.com/geoffreynyaga/multipass-run)
- [levalleyjack/multipass-manager-vscode](https://github.com/levalleyjack/multipass-manager-vscode)


# Copyright

The code in this repository is licensed under GNU General Public License v3.0.
See [LICENSE](https://github.com/canonical/multipass/blob/main/LICENSE) for more information.

<!-- references for status badges -->
[gha-image-linux]: https://github.com/canonical/multipass/actions/workflows/linux.yml/badge.svg?branch=main
[gha-url-linux]: https://github.com/canonical/multipass/actions/workflows/linux.yml
[gha-image-windows]: https://github.com/canonical/multipass/actions/workflows/windows.yml/badge.svg?branch=main
[gha-url-windows]: https://github.com/canonical/multipass/actions/workflows/windows.yml
[gha-image-macos]: https://github.com/canonical/multipass/actions/workflows/macos.yml/badge.svg?branch=main
[gha-url-macos]: https://github.com/canonical/multipass/actions/workflows/macos.yml

[snap-image]: https://snapcraft.io/multipass/badge.svg
[snap-url]: https://snapcraft.io/multipass

[codecov-image]: https://codecov.io/gh/canonical/multipass/branch/main/graph/badge.svg
[codecov-url]: https://codecov.io/gh/canonical/multipass
