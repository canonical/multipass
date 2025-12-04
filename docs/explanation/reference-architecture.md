(explanation-reference-architecture)=
# Reference architecture

```{seealso}
[Mount](explanation-mount), [GUI Client](reference-gui-client), [Instance](explanation-instance)
```

Multipass is a tool to generate cloud-style Ubuntu VMs quickly on Linux, macOS and Windows. It provides a GUI and a simple but powerful CLI, enabling you to quickly access an Ubuntu command line or create your own local mini-cloud.

```{figure} /images/multipass-reference-architecture.jpg
   :alt: Multipass reference architecture diagram
```

## Clients

A term we borrow from the client/server model. Multipass clients provide users with an interface to Multipass and its instances, delegating requests on the Multipass daemon. There are two clients in Multipass, both offering a user interface: one on the command line — the Command Line Interface, or CLI — and one graphical — The Graphical User Interface, or GUI.

## CLI (Command Line Interface)

Is one of the two user interfaces that Multipass offers, in a client program that communicates with the daemon and which can be invoked on the command line: multipass. It is through this client that users can control Multipass and its instances in the terminal, with commands like multipass launch or `multipass start`.

## GUI (Graphical User Interface)

The Multipass GUI provides a visual, point-and-click interface for managing Multipass and its instances. It runs as a separate client application which (like the CLI) communicates with, and delegates operations to, the Multipass daemon. The Multipass GUI provides a visual, point-and-click interface for managing Multipass and its instances.

## Daemon/Service

A long living background process that is responsible for a variety of tasks, like managing instances, client authentication, storing configuration settings, or providing file system shares. The daemon provides the services that the clients use. It runs as a privileged user allowing it to interact directly with system resources and control which users have access to Multipass’s instances.

## Storage/Mounts

A filesystem share between the host machine and a guest instance. Mounts expose host directories inside the virtual machine and can be used for bidirectional file transfer. The underlying technology depends on the mount type and host platform, and it can be either SSHFS/SFTP, 9P, or SMB.

## Instances
A Linux virtual machine hosted by the user’s machine. Multipass uses a hypervisor technology specific to the user’s native operating system which is used to emulate the instance.

## Image Hosts

Remote source to obtain disk images from. As of now, these are public online repositories of cloud Linux images that Multipass creates new instances from. Multipass periodically fetches and updates the image metadata from image hosts, pruning old images and downloading new ones on demand.

## Networking

Multipass creates a virtual network using a dedicated subnet on the host. Each instance obtains an IP address from this private network, via DHCP. Within this private network, instances can initiate outbound connections (egress), communicate with each other, and be reached from the host, but they remain inaccessible from outside the host. For that, instances can be bridged to the host's physical network, allowing inbound connections (ingress) from other nodes (e.g. other computers on a home network).

## Web

Multipass instances are online by default (provided the host is). They can reach the web via either the private or the public network. The Multipass daemon also fetches information from the web, in particular image hosts.
