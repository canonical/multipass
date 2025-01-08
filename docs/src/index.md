(index)=

<!--Welcome to the *Multipass Guide!*

Multipass is a mini-cloud on your workstation using native hypervisors of all the supported plaforms (Windows, macOS and Linux), it will give you an Ubuntu command line in just a click ("Open shell") or a simple `multipass shell` command, or even a keyboard shortcut. Find what images are available with `multipass find` and create new instances with `multipass launch`.

You can initialize instances through [cloud-init](https://cloudinit.readthedocs.io/en/latest/) as you normally would on all the clouds Ubuntu is supported on, just pass the configuration to `multipass launch --cloud-init`.

Accessing files from your host machine is supported through the `multipass mount` command, and to move files between the host and instances, you can use `multipass transfer`.

Please learn more details in the linked documentation topics.
-->

Multipass is a tool to generate cloud-style Ubuntu VMs quickly on Linux, macOS and Windows. It provides a simple but powerful CLI that enables you to quickly access an Ubuntu command line or create your own local mini-cloud.

Local development and testing can be challenging, but Multipass simplifies these processes by automating setup and teardown. Multipass has a growing library of images that you can use to launch purpose-built VMs or custom VMs you’ve configured yourself through its powerful `cloud-init` interface.

Developers can use Multipass to prototype cloud deployments and to create fresh, customized Linux dev environments on any machine. Multipass is the quickest way for Mac and Windows users to get an Ubuntu command line on their systems. You can also use it as a sandbox to try new things without affecting your host machine or requiring a dual boot.

---

## In this documentation

|                                                                                                      |                                                                                             |
|------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
| [Tutorial](/tutorial)</br>  Get started - a hands-on introduction to Multipass for new users </br> | [How-to guides](/how-to-guides/index) </br> Step-by-step guides covering key operations and common tasks |
| [Explanation](/explanation/index) </br> Concepts - discussion and clarification of key topics                   | [Reference](/reference/index) </br> Technical information - specifications, APIs, architecture       |

---

## Project and community

We value your input and contributions! Here are some ways you can join our community or get help with your Multipass questions:

* Read our [Code of Conduct](https://ubuntu.com/community/code-of-conduct)
* Read our quick guide: [Contribute to Multipass docs](/contribute-to-multipass-docs)
* Join the [Discourse forum](https://discourse.ubuntu.com/c/multipass/21)
* Report an issue or contribute to the code on [GitHub](https://github.com/canonical/multipass/issues)


```{toctree}
:hidden:
:titlesonly:
:maxdepth: 2
:glob:

Home <self>
/tutorial*/index
/how*/index
/reference*/index
/explanation*/index
*
