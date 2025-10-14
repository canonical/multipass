(how-to-guides-manage-instances-create-an-instance)=
# Create an instance

> See also: [`launch`](/reference/command-line-interface/launch), [Instance](/explanation/instance)

This document demonstrates various ways to create an instance with Multipass. While every method is a one-liner involving the command `multipass launch`, each showcases a different option that you can use to get exactly the instance that you want.

## Create an instance

> See also: [`launch`](/reference/command-line-interface/launch), [`info`](/reference/command-line-interface/info)

To create an instance with Multipass, run the command `multipass launch`. This launches a new instance, which is randomly named; for example:

```{code-block} text
...
Launched: keen-yak
```

In particular, when we run `multipass info keen-yak`, we find out that it is an Ubuntu LTS release, namely 18.04, with 1GB RAM, 1 CPU, 5GB of disk:

```{code-block} text
Name:           keen-yak
State:          RUNNING
IPv4:           10.140.94.253
Release:        Ubuntu 18.04.1 LTS
Image hash:     d53116c67a41 (Ubuntu 18.04 LTS)
CPU(s):         1
Load:           0.00 0.12 0.18
Disk usage:     1.1G out of 4.7G
Memory usage:   71.6M out of 985.4M
```

## Create an instance with a specific image

> See also: [`find`](/reference/command-line-interface/find), [`launch <image>`](/reference/command-line-interface/launch), [`info`](/reference/command-line-interface/info)

To find out which images are available, run `multipass find`. Here's a sample output:

```{code-block} text
Image                       Aliases           Version          Description
20.04                       focal             20240821         Ubuntu 20.04 LTS
22.04                       jammy             20240912         Ubuntu 22.04 LTS
24.04                       noble,lts         20240911         Ubuntu 24.04 LTS

Blueprint                   Aliases           Version          Description
anbox-cloud-appliance                         latest           Anbox Cloud Appliance
charm-dev                                     latest           A development and testing environment for charmers
docker                                        0.4              A Docker environment with Portainer and related tools
jellyfin                                      latest           Jellyfin is a Free Software Media System that puts you in control of managing and streaming your media.
minikube                                      latest           minikube is local Kubernetes
ros-noetic                                    0.1              A development and testing environment for ROS Noetic.
ros2-humble                                   0.1              A development and testing environment for ROS 2 Humble.
```

To launch an instance with a specific image, include the image name or alias in the command, for example `multipass launch jammy`:

```{code-block} text
...
Launched: tenacious-mink
```

`multipass info tenacious-mink` confirms that we've launched an instance of the selected image.

```{code-block} text
Name:           tenacious-mink
State:          Running
Snapshots:      0
IPv4:           192.168.64.22
Release:        Ubuntu 22.04.5 LTS
Image hash:     e898c1c93b32 (Ubuntu 22.04 LTS)
CPU(s):         1
Load:           0.00 0.02 0.01
Disk usage:     1.6GiB out of 4.8GiB
Memory usage:   149.5MiB out of 962.2MiB
Mounts:         --
```

## Create an instance with a custom name

> See also: [`launch --name`](/reference/command-line-interface/launch)

To launch an instance with a specific name, add the `--name` option to the command line; for example `multipass launch kinetic --name helpful-duck`:

```{code-block} text
...
Launched: helpful-duck
```

## Create an instance with custom CPU number, disk and RAM

> See also: [`launch --cpus --disk --memory`](/reference/command-line-interface/launch)

You can specify a custom number of CPUs, disk and RAM size using the `--cpus`, `--disk` and `--memory` arguments, respectively. For example:

```{code-block} text
multipass launch --cpus 4 --disk 20G --memory 8G
```

## Create an instance with primary status

> See also: [`launch --name primary`](/reference/command-line-interface/launch)

An instance can obtain the primary status at creation time if its name is `primary`:

```{code-block} text
multipass launch kinetic --name primary
```

For more information, see [How to use the primary instance](/how-to-guides/manage-instances/use-the-primary-instance).

(create-an-instance-with-multiple-network-interfaces)=
## Create an instance with multiple network interfaces

> See also: [`launch --network`](/reference/command-line-interface/launch)

Multipass can create instances with additional network interfaces using the `multipass launch` command with the `--network` option. This is complemented by the [`networks`](/reference/command-line-interface/networks) command, that you can use to find available host networks to bridge with.

This feature is only supported for images with [`cloud-init` support for v2 network config](https://cloudinit.readthedocs.io/en/latest/topics/network-config-format-v2.html), which in turn requires [netplan](https://netplan.io/) to be installed, meaning that you'll require Ubuntu 17.10 and Ubuntu Core 16 (except `snapcraft:core16`) or later. More specifically, this feature is only supported in the following scenarios:

* on Linux, with QEMU (*from Multipass 1.15 onward*)
* on Windows, with both Hyper-V and VirtualBox
* on macOS, with the QEMU and VirtualBox drivers

The `--network` option can be given multiple times to request multiple network interfaces beyond the default one, which is always present. Each time you add the `--network` option you also need to provide an argument specifying the properties of the desired interface:

- `name` - This is the only required value, used to identify the host network to connect the instance's device to (see [`networks`](/reference/command-line-interface/networks) for possible values).
- `mode` - Either `auto` (default) or `manual`; with `auto`, the instance will attempt to configure the network automatically.
- `mac` - Custom MAC address to use for the device.

These properties can be specified in the format `<key>=<value>` but a simpler form with only `<name>` is available for the most common use case. Here is an example:

```{code-block} text
multipass launch --network en0 --network name=bridge0,mode=manual
```

You can inspect the IP addresses assigned to the network interfaces of the new instance ("upbeat-whipsnake) on the system using the command:

```{code-block} text
multipass exec upbeat-whipsnake -- ip -br address show scope global
```

Sample output:

```{code-block} text
enp0s3           UP             10.0.2.15/24
enp0s8           UP             192.168.1.146/24
enp0s9           DOWN
```

Last, you can run `ping -c1 192.168.1.146` from the same network to verify that the IP can be reached:

```{code-block} text
PING 192.168.1.146 (192.168.1.146): 56 data bytes
64 bytes from 192.168.1.146: icmp_seq=0 ttl=64 time=0.378 ms
...
```

In the example above, we got the following interfaces inside the instance:
- `enp0s3` - The default interface, that the instance can use to reach the outside world and that Multipass uses to communicate with the instance.
- `enp0s8` - The interface that is connected to `en0` on the host and that is automatically configured.
- `enp0s9` - The interface that is connected to `bridge0` on the host, ready for manual configuration.

### Routing

Extra interfaces are configured with a higher metric (200) than the default one (100). So, by default the instance will only route through them if they're a better match for the destination IP (see [Wikipedia | Longest_prefix-match](https://en.wikipedia.org/wiki/Longest_prefix_match).

For example, if the command `multipass exec upbeat-whipsnake -- ip route` returns the following routing table:

```{code-block} text
default via 10.0.2.2 dev enp0s3 proto dhcp src 10.0.2.15 metric 100
default via 192.168.1.1 dev enp0s8 proto dhcp src 192.168.1.146 metric 200
10.0.2.0/24 dev enp0s3 proto kernel scope link src 10.0.2.15
10.0.2.2 dev enp0s3 proto dhcp scope link src 10.0.2.15 metric 100
192.168.1.0/24 dev enp0s8 proto kernel scope link src 192.168.1.146
192.168.1.1 dev enp0s8 proto dhcp scope link src 192.168.1.146 metric 200
```

you can then explore how specific IPs are routed:

```{code-block} text
multipass exec upbeat-whipsnake -- ip route get 91.189.88.181
```

In this case, for example:
```{code-block} text
91.189.88.181 via 10.0.2.2 dev enp0s3 src 10.0.2.15 uid 1000
    cache
```

### Bridging

On Linux, when trying to connect an instance network to an ethernet device on the host, Multipass will offer to create the required bridge.

First, run the `multipass networks` command; for example:

```{code-block} text
Name             Type      Description
eth0             ethernet  Ethernet device
mpbr0            bridge    Network bridge for Multipass
virbr0           bridge    Network bridge
```

Then, select an ethernet device and launch a new instance requesting to connect to it, for example via `multipass launch --network eth0`. The output will be:

```{code-block} text
Multipass needs to create a bridge to connect to eth0.
This will temporarily disrupt connectivity on that interface.

Do you want to continue (yes/no)?
```

However, Multipass requires `NetworkManager` to achieve this. On installations that do not have `NetworkManager` installed (e.g. Ubuntu Server), the user can still create a bridge by other means and pass that to Multipass. For instance, this configuration snippet achieves that with Netplan:

```{code-block} yaml
network:
  bridges:
    mybridge:
      dhcp4: true
      interfaces:
        - eth0
```

That goes somewhere in `/etc/netplan/` (e.g. `/etc/netplan/50-custom.yaml`). After a successful `netplan try` or `netplan apply`, Multipass will show the new bridge with the `networks` command and instances can be connected to it:

```{code-block} text
multipass launch --network mybridge
```

Another option is to install `NetworkManager`, but other network handlers need to be deactivated to avoid conflicts and make the new bridges permanent.

## Create an instance with a custom DNS

In some scenarios the default of using the system-provided DNS will not be sufficient. When that's the case, you can use the `--cloud-init` option to the [`launch`](/reference/command-line-interface/launch) command, or modify the networking configuration after the instance started.

### The `--cloud-init` approach

> See also: [`launch --cloud-init`](/reference/command-line-interface/launch)

To use a custom DNS in your instances, you can use this `cloud-init` snippet:

```{code-block} yaml
#cloud-config
bootcmd:
- printf "[Resolve]\nDNS=8.8.8.8" > /etc/systemd/resolved.conf
- [systemctl, restart, systemd-resolved]
```

Replace `8.8.8.8` with whatever your preferred DNS server is. You can then launch the instance using the following command:

```{code-block} text
multipass launch --cloud-init systemd-resolved.yaml
```

### The Netplan approach

After the instance booted, you can modify the `/etc/netplan/50-cloud-init.yaml` file, adding the `nameservers` entry:

```{code-block} yaml
network:
  ethernets:
    ens3:
      dhcp4: true
      match:
        macaddress: 52:54:00:fe:52:ee
     set-name: ens3
     nameservers:
       search: [mydomain]
       addresses: [8.8.8.8]</b>
```

You can then test it with the command `sudo netplan try`:

```{code-block} text
Do you want to keep these settings?

Press ENTER before the timeout to accept the new configuration

Changes will revert in 120 seconds
...
```
