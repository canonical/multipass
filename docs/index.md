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

### Basics

````{domain}

```{slice} Getting started

{doc}`Install Multipass <how-to-guides/install-multipass>`
{doc}`Tutorial <tutorial/index>`
```

```{slice} CLI and GUI interface

{doc}`Command-line interface <reference/command-line-interface/index>`
{doc}`GUI client <reference/gui-client>`
```

```{slice} Use a VM: exec and shell

{doc}`Multipass exec and shells <explanation/multipass-exec-and-shells>`
{doc}`Use an instance <how-to-guides/manage-instances/use-an-instance>` domain
```

````

### Core Concepts

````{domain}

```{slice} Virtualization / Virtual machines

{doc}`Host <explanation/index>`
{doc}`Instance <explanation/instance>`
{doc}`Platform <explanation/platform>`
```

```{slice} Manage instances

{doc}`Create an instance <how-to-guides/manage-instances/create-an-instance>` domain
{doc}`Modify an instance <how-to-guides/manage-instances/modify-an-instance>` domain
{doc}`Remove an instance <how-to-guides/manage-instances/remove-an-instance>` domain
{doc}`Instance states <reference/instance-states>`
```

```{slice} Images

{doc}`Image <explanation/image>`
{doc}`Build Multipass images with Packer <how-to-guides/customise-multipass/build-multipass-images-with-packer>`
```

````

### Multipass and its host

````{domain}

```{slice} Native drivers

{doc}`Driver <explanation/driver>`
{doc}`Set up the driver <how-to-guides/customise-multipass/set-up-the-driver>` domain
```

```{slice} Multipass architecture

{doc}`Service <explanation/service>`
{doc}`Reference architecture <explanation/reference-architecture>`
{doc}`Authenticate users with the Multipass service <how-to-guides/customise-multipass/authenticate-users-with-the-multipass-service>`
{doc}`Authentication <explanation/authentication>` domain
```

```{slice} Share data with the host

{doc}`Mount <explanation/mount>`
{doc}`Share data with an instance <how-to-guides/manage-instances/share-data-with-an-instance>`
{doc}`Transfer files <reference/command-line-interface/transfer>`
{doc}`Security considerations <explanation/mount>`
{doc}`ID mapping <explanation/id-mapping>`
```

```{slice} Network

{doc}`Add a network to an existing instance <how-to-guides/manage-instances/add-a-network-to-an-existing-instance>`
{doc}`Configure static IPs <how-to-guides/manage-instances/configure-static-ips>`
{doc}`Set up custom networking <how-to-guides/manage-instances/set-up-custom-networking>`
```

````

### Features

````{domain}

```{slice} Instance customization

{doc}`Launch customized instances with Multipass and cloud-init <how-to-guides/manage-instances/launch-customized-instances-with-multipass-and-cloud-init>`
```

```{slice} Aliases

{doc}`Alias <explanation/index>`
{doc}`Use instance command aliases <how-to-guides/manage-instances/use-instance-command-aliases>` domain
```

```{slice} Primary instance

{doc}`Use the primary instance <how-to-guides/manage-instances/use-the-primary-instance>` domain
```

````

### Lifecycle

````{domain}

```{slice} Manage instances

{doc}`Create an instance <how-to-guides/manage-instances/create-an-instance>` domain
{doc}`Use an instance <how-to-guides/manage-instances/use-an-instance>` domain
{doc}`Modify an instance <how-to-guides/manage-instances/modify-an-instance>` domain
{doc}`Use the primary instance <how-to-guides/manage-instances/use-the-primary-instance>` domain
{doc}`Use instance command aliases <how-to-guides/manage-instances/use-instance-command-aliases>` domain
{doc}`Remove an instance <how-to-guides/manage-instances/remove-an-instance>` domain
```

```{slice} Settings

{doc}`Settings <reference/settings/index>`
{doc}`Settings keys and values <explanation/settings-keys-values>`
{doc}`Instance name format <reference/instance-name-format>`
```

```{slice} Instances: snapshots, cloning

{doc}`Snapshot <explanation/snapshot>`
{doc}`snapshot <reference/command-line-interface/snapshot>`
{doc}`clone <reference/command-line-interface/clone>`
```

```{slice} Uninstallation

{doc}`Uninstall Multipass <how-to-guides/install-multipass>`
```

````

### Quality

````{domain}

```{slice} Logs and log levels

{doc}`Access logs <how-to-guides/troubleshoot/access-logs>`
{doc}`Logging levels <reference/logging-levels>`
{doc}`Configure Multipass's default logging level <how-to-guides/customise-multipass/configure-multipass-default-logging-level>`
```

```{slice} Security

{doc}`About security <explanation/about-security>`
{doc}`Authentication <explanation/authentication>` domain
```

```{slice} Performance

{doc}`About performance <explanation/about-performance>`
```

````

### Special use-cases, concerns, and problems

````{domain}

```{slice} Use-cases

{doc}`Set up a graphical interface <how-to-guides/customise-multipass/set-up-a-graphical-interface>`
```

```{slice} Troubleshooting

{doc}`Troubleshoot launch/start issues <how-to-guides/troubleshoot/troubleshoot-launch-start-issues>`
{doc}`Mount an encrypted home folder <how-to-guides/troubleshoot/mount-an-encrypted-home-folder>`
{doc}`Troubleshoot networking <how-to-guides/troubleshoot/troubleshoot-networking>`
```

```{slice} Migration

{doc}`Migrate from Hyperkit to QEMU on macOS <how-to-guides/customise-multipass/migrate-from-hyperkit-to-qemu-on-macos>`
```

```{slice} Platform specific

{doc}`Set up the driver <how-to-guides/customise-multipass/set-up-the-driver>` domain
{doc}`Integrate with Windows Terminal <how-to-guides/customise-multipass/integrate-with-windows-terminal>`
{doc}`Use a different terminal from the system icon <how-to-guides/customise-multipass/use-a-different-terminal-from-the-system-icon>`
```

````

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
