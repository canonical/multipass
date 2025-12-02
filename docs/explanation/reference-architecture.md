(explanation-reference-architecture)=
# Reference architecture

```{seealso}
[Mount](explanation-mount), [GUI Client](reference-gui-client), [Instance](explanation-instance)
```

Multipass is a tool to generate cloud-style Ubuntu VMs quickly on Linux, macOS and Windows. It provides a simple but powerful CLI that enables you to quickly access an Ubuntu command line or create your own local mini-cloud.

```{figure} /images/multipass-reference-architecture.jpg
   :alt: Multipass reference architecture diagram
```

## Clients

A term we borrow from the client/server model. Multipass clients provide users with an interface to Multipass and its instances, delegating requests on the Multipass daemon. There are two clients in Multipass, both offering a user interface: CLI and GUI.

## CLI (Command Line Interface)

Is one of the two user interfaces that Multipass offers, in a client program that communicates with the daemon and which can be invoked on the command line: multipass. It is through this client that users can control Multipass and its instances in the terminal, with commands like multipass launch or multipass startstart.

## GUI (Graphical User Interface)

The Multipass GUI provides a visual, point-and-click interface for managing Multipass and its instances. It runs as a separate client application that communicates with, and delegates operations to, the Multipass daemon. The Multipass GUI provides a visual, point-and-click interface for managing Multipass and its instances. It runs as a separate client application that communicates with, and delegates operations to, the Multipass daemon.

## Daemon/Service

A long living process responsible for a variety of tasks; managing instances, client authentication, storing configuration settings, and providing file system shares. The daemon runs as a privileged user allowing it to interact directly with system resources and control which users have access to Multipass’s instances.

## Storage/Mounts

A filesystem share between the host machine and guest instance. Mounts are provisioned through SSH File System (SSHFS) and accurately translate ownership and user permissions between the host OS and the Linux guest.
Instances - A Linux virtual machine hosted by the user’s machine. Multipass uses a hypervisor technology specific to the user’s native operating system which is used to emulate the instance.

## Image Hosts

Remote source to obtain disk images from. As of now, these are public online repositories of cloud Linux images that Multipass creates new instances from. Multipass periodically fetches and updates the image metadata from image hosts, pruning old images and downloading new ones on demand.

## Networking

Multipass creates a local network for virtual machines to communicate. This network has a random private subnet assigned to it, and each virtual machine would obtain an IP address via DHCP from this network by default. The virtual machines are accessible to each other through the private network. This virtual network is bridged to the primary host network by default to provide web connectivity to the VMs (meaning all the VMs are behind a NAT).

## Web

Multipass communicates with Web servers to obtain image metadata and perform latest version checks. All communication between the web and Multipass occurs through the daemon, and it’s one-way in the sense that Multipass only consumes the data.
