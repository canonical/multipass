(how-to-guides-index)=
# How-to guides

The following how-to guides provide step-by-step instructions on the installation, use, management and troubleshooting of Multipass.

## Install and deploy Multipass

Installing Multipass is a straightforward process but may require some prerequisite steps, depending on your host system. You can find specific installation instructions for your operating system in this guide:
- [How to install Multipass](install-multipass)

## Manage instances

Multipass allows you to create Ubuntu instances with a single command. As your needs grow, you can modify and customise instances as well as use and create blueprints for customised instances: <!--- This line added by @nielsenjared -->

- [Create an instance](manage-instances/create-an-instance)
- [Modify an instance](manage-instances/modify-an-instance)
- [Use an instance](manage-instances/use-an-instance)
- [Use the primary instance](manage-instances/use-the-primary-instance)
- [Use instance command aliases](manage-instances/use-instance-command-aliases)
- [Share data with an instance](manage-instances/share-data-with-an-instance)
- [Remove an instance](manage-instances/remove-an-instance)
- [Add a network to an existing instance](manage-instances/add-a-network-to-an-existing-instance)
- [Configure static IPs](manage-instances/configure-static-ips)
- [Use a blueprint](manage-instances/use-a-blueprint)
- [Use the Docker blueprint](manage-instances/use-the-docker-blueprint)
- [Run a Docker container in Multipass](manage-instances/run-a-docker-container-in-multipass)

## Customise Multipass

You may also want to customise Multipass to address specific needs, from managing Multipass drivers to configuring a graphical user interface:

- [Set up the driver](customise-multipass/set-up-the-driver)
- [Migrate from Hyperkit to QEMU on macOS](customise-multipass/migrate-from-hyperkit-to-qemu-on-macos)
- [Authenticate clients with the Multipass service](customise-multipass/authenticate-clients-with-the-multipass-service)
- [Build Multipass images with Packer](customise-multipass/build-multipass-images-with-packer)
- [Set up a graphical interface](customise-multipass/set-up-a-graphical-interface)
- [Use a different terminal from the system icon](customise-multipass/use-a-different-terminal-from-the-system-icon)
- [Integrate with Windows Terminal](customise-multipass/integrate-with-windows-terminal)
- [Configure where Multipass stores external data](customise-multipass/configure-where-multipass-stores-external-data)
- [Configure Multipass’s default logging level](customise-multipass/configure-multipass-default-logging-level)

<!-- REMOVED FROM DOCS AND MOVED TO COMMUNITY KNOWLEDGE
- [Use Multipass remotely](/)
-->

## Troubleshoot

Use the following how-to guides to troubleshoot issues with your Multipass installation, beginning by inspecting the logs: <!--- This line added by @nielsenjared -->

- [Access logs](troubleshoot/access-logs)
- [Mount an encrypted home folder](troubleshoot/mount-an-encrypted-home-folder)
- [Troubleshoot launch/start issues](troubleshoot/troubleshoot-launch-start-issues)
- [Troubleshoot networking](troubleshoot/troubleshoot-networking)

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/how-to-guides" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*


```{toctree}
:hidden:
:titlesonly:
:maxdepth: 2
:glob:

install-multipass
manage-instances/index
customise-multipass/index
troubleshoot/index
