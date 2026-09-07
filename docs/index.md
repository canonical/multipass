(index)=
# Multipass

<!--Welcome to the *Multipass Guide!*

Multipass is a mini-cloud on your workstation using native hypervisors of all the supported plaforms (Windows, macOS and Linux), it will give you an Ubuntu command line in just a click ("Open shell") or a simple `multipass shell` command, or even a keyboard shortcut. Find what images are available with `multipass find` and create new instances with `multipass launch`.

You can initialise instances through [cloud-init](https://cloudinit.readthedocs.io/en/latest/) as you normally would on all the clouds Ubuntu is supported on, just pass the configuration to `multipass launch --cloud-init`.

Accessing files from your host machine is supported through the `multipass mount` command, and to move files between the host and instances, you can use `multipass transfer`.

Please learn more details in the linked documentation topics.
-->

Multipass is a tool to generate cloud-style virtual machines (VMs) quickly on Linux, macOS and Windows. It provides a simple but powerful CLI that enables you to quickly access an Ubuntu command line, launch instances of other Linux distributions, or create your own local mini-cloud.

Local development and testing can be challenging, but Multipass simplifies these processes by automating setup and teardown. Multipass has a growing library of images that you can use to launch purpose-built VMs or custom VMs you’ve configured yourself through its powerful `cloud-init` interface.

Developers can use Multipass to prototype cloud deployments and to create fresh, customised Linux dev environments on any machine. Multipass is the quickest way for Mac and Windows users to get an Ubuntu command line on their systems. You can also use it as a sandbox to try new things without affecting your host machine or requiring a dual boot.

---

## In this documentation

### Basic / Getting started

- [Install Multipass](how-to-guides-install-multipass) • [Tutorial](tutorial-index)
- **CLI and GUI interface:** [Command-line interface](reference-command-line-interface-index) • [GUI client](reference-gui-client)
- **Use a VM: exec and shell:** [Multipass exec and shells](explanation-multipass-exec-and-shells) • [Use an instance](how-to-guides-manage-instances-use-an-instance)

### Core Concepts

- **Virtualization / Virtual machines:** [Host](explanation-host) • [Instance](explanation-instance) • [Platform](explanation-platform)
- **Manage instances:** [Create an instance](how-to-guides-manage-instances-create-an-instance) • [Modify an instance](how-to-guides-manage-instances-modify-an-instance) • [Remove an instance](how-to-guides-manage-instances-remove-an-instance) • [Instance states](reference-instance-states)
- **Images:** [Image](explanation-image) • [Build Multipass images with Packer](how-to-guides-customise-multipass-build-multipass-images-with-packer)

### Multipass and its host

- **Native drivers:** [Driver](explanation-driver) • [Set up the driver](how-to-guides-customise-multipass-set-up-the-driver)
- **Multipass architecture:** [Service](explanation-service) • [Reference architecture](explanation-reference-architecture) • [Authenticate users with the Multipass service](how-to-guides-customise-multipass-authenticate-users-with-the-multipass-service) • [Authentication](explanation-authentication)
- **Share data with the host:** [Mount](explanation-mount) • [Share data with an instance](how-to-guides-manage-instances-share-data-with-an-instance) • [Transfer files](reference-command-line-interface-transfer) • [Security considerations](security-considerations-mount) • [ID mapping](explanation-id-mapping)
- **Network:** [Add a network to an existing instance](how-to-guides-manage-instances-add-a-network-to-an-existing-instance) • [Configure static IPs](how-to-guides-manage-instances-configure-static-ips) • [Set up custom networking](how-to-guides-manage-instances-set-up-custom-networking)

### Features

- **Instance customization:** [Launch customized instances with Multipass and cloud-init](how-to-guides-manage-instances-launch-customized-instances-with-multipass-and-cloud-init)
- **Aliases:** [Alias](explanation-alias) • [Use instance command aliases](how-to-guides-manage-instances-use-instance-command-aliases)
- **Primary instance:** [Use the primary instance](how-to-guides-manage-instances-use-the-primary-instance)

### Lifecycle

- **Manage instances:** [Create an instance](how-to-guides-manage-instances-create-an-instance) • [Use an instance](how-to-guides-manage-instances-use-an-instance) • [Modify an instance](how-to-guides-manage-instances-modify-an-instance) • [Use the primary instance](how-to-guides-manage-instances-use-the-primary-instance) • [Use instance command aliases](how-to-guides-manage-instances-use-instance-command-aliases) • [Remove an instance](how-to-guides-manage-instances-remove-an-instance)
- **Settings:** [Settings](reference-settings-index) • [Settings keys and values](explanation-settings-keys-values) • [Instance name format](reference-instance-name-format)
- **Instances: snapshots, cloning:** [Snapshot](explanation-snapshot) • [`snapshot`](reference-command-line-interface-snapshot) • [`clone`](reference-command-line-interface-clone)
- [Uninstall Multipass](how-to-guides-install-multipass-uninstall)

### Quality

- **Logs and log levels:** [Access logs](how-to-guides-troubleshoot-access-logs) • [Logging levels](reference-logging-levels) • [Configure Multipass's default logging level](how-to-guides-customise-multipass-configure-multipass-default-logging-level)
- **Security:** [About security](explanation-about-security) • [Authentication](explanation-authentication)
- **Performance:** [About performance](explanation-about-performance)

### Special use-cases, concerns, and problems

- **Use-cases:** [Set up a graphical interface](how-to-guides-customise-multipass-set-up-a-graphical-interface)
- **Troubleshooting:** [Troubleshoot launch/start issues](how-to-guides-troubleshoot-troubleshoot-launch-start-issues) • [Mount an encrypted home folder](how-to-guides-troubleshoot-mount-an-encrypted-home-folder) • [Troubleshoot networking](how-to-guides-troubleshoot-troubleshoot-networking)
- **Migration:** [Migrate from Hyperkit to QEMU on macOS](how-to-guides-customise-multipass-migrate-from-hyperkit-to-qemu-on-macos)
- **Platform specific:** [Set up the driver](how-to-guides-customise-multipass-set-up-the-driver) • [Integrate with Windows Terminal](how-to-guides-customise-multipass-integrate-with-windows-terminal) • [Use a different terminal from the system icon](how-to-guides-customise-multipass-use-a-different-terminal-from-the-system-icon)

---

## How this documentation is organized

This documentation uses the [Diátaxis documentation structure](https://diataxis.fr/).

- [Tutorial](tutorial-index) takes you step-by-step through your first Multipass workflow, from installation to launching and working with instances.

- [How-to guides](how-to-guides-index) assume you have basic familiarity with Multipass. They cover practical tasks such as instance management, configuration, networking, and troubleshooting.

- [Reference](reference-index) provides technical details on CLI commands, settings, architecture, and platform-specific behavior.

- [Explanation](explanation-index) includes conceptual overviews, background context, and deeper discussion of how Multipass works.

---

## Project and community

We value your input and contributions! Here are some ways you can join our community or get help with your Multipass questions:

* Read our [Code of Conduct](https://ubuntu.com/community/code-of-conduct)
* Read our quick guide: {ref}`contribute-to-multipass-docs`
* Join the [Discourse forum](https://discourse.ubuntu.com/c/project/multipass/21/)
* Report an issue or contribute to the code on [GitHub](https://github.com/canonical/multipass/issues)


```{toctree}
:hidden:
:titlesonly:
:maxdepth: 2

Home <self>
tutorial/index
how-to-guides/index
reference/index
explanation/index
reference/release-notes/index
contribute-to-multipass-docs
```
