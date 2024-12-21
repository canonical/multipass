# Set up the driver
> See also: [Driver](/explanation/driver), [`local.driver`](/reference/settings/local-driver)

This document demonstrates how to choose, set up, and manage the drivers behind Multipass. Multipass already has sensible defaults, so this is an optional step. 

## Default driver

[tabs]

[tab version="Linux"]

By default, Multipass on Linux uses the `qemu` or `lxd` driver (depending on the architecture). 

[/tab]

[tab version="macOS"]

By default, Multipass on macOS uses the `qemu` driver. 

[/tab]

[tab version="Windows"]

By default, Multipass on Windows uses the `hyperv` driver. 

[/tab]

[/tabs]

## Install an alternative driver

[tabs]

[tab version="Linux"]

If you want more control over your VMs after they are launched, you can also use the experimental [libvirt](https://libvirt.org/) driver. 

To install libvirt, run the following command (or use the equivalent for your Linux distribution):

```plain
sudo apt install libvirt-daemon-system
```

Now you can switch the Multipass driver to libvirt. First, enable Multipass to use your local libvirt by connecting to the libvirt interface/plug:

```plain
sudo snap connect multipass:libvirt
```

Then, stop all instances and tell Multipass to use libvirt, running the following commands:

```plain
multipass stop --all
multipass set local.driver=libvirt
```

All your existing instances will be migrated and can be used straight away.

[note type="information"]
You can still use the `multipass` client and the tray icon, and any changes you make to the configuration of the instance in libvirt will be persistent. They may not be represented in Multipass commands such as `multipass info`, though.
```

[/tab]

[tab version="macOS"]

An alternative option is to use VirtualBox.

To switch the Multipass driver to VirtualBox, run this command:

```plain
sudo multipass set local.driver=virtualbox
```

From now on, all instances started with `multipass launch` will use VirtualBox behind the scenes.

[/tab]

[tab version="Windows"]

If you want to (or have to), you can change the hypervisor that Multipass uses to VirtualBox. 

To that end, install VirtualBox, if you haven't yet. You may find that you need to [run the VirtualBox installer as administrator](https://forums.virtualbox.org/viewtopic.php?f=6&t=88405#p423658). 

To switch the Multipass driver to VirtualBox (also with Administrator privileges), run this command:

```powershell
multipass set local.driver=virtualbox
```

From then on, all instances started with `multipass launch` will use VirtualBox behind the scenes.

[/tab]

[/tabs]

## Use the driver to view Multipass instances

[tabs]

[tab version="Linux"]

You can view instances with libvirt in two ways, using the `virsh` CLI or the [`virt-manager` GUI](https://virt-manager.org/).

To use the `virsh` CLI, launch an instance and then run the command `virsh list` (see [`man virsh`](http://manpages.ubuntu.com/manpages/xenial/man1/virsh.1.html) for a command reference):

```plain
virsh list                             
```

The output will be similar to the following:

```plain      
 Id   Name                   State
