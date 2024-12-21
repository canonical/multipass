# Configure static IPs
> See also: [Instance](/explanation/instance)

This guide explains how to create instances with static IPs in a new network, internal to the host. With this approach, instances get an extra IP that does not change with restarts. By using a separate, local network we avoid any IP conflicts. Instances retain the usual default interface with a DHCP-allocated IP, which gives them connectivity to the outside.

## Step 1: Create a Bridge

The first step is to create a new bridge/switch with a static IP on your host. 

This is beyond the scope of Multipass but, as an example, here is how this can be achieved with NetworkManager on Linux (e.g. on Ubuntu Desktop):

```
nmcli connection add type bridge con-name localbr ifname localbr \
    ipv4.method manual ipv4.addresses 10.13.31.1/24
```

This will create a bridge named `localbr` with IP `10.13.31.1/24`. You can see the new device and address with `ip -c -br addr show dev localbr`. This should show:

```
localbr           DOWN           10.13.31.1/24
```

You can also run `multipass networks` to confirm the bridge is available for Multipass to connect to.

## Step 2: Launch an instance with a manual network

Next we launch an instance with an extra network in manual mode, connecting it to this bridge:

```
multipass launch --name test1 --network name=localbr,mode=manual,mac="52:54:00:4b:ab:cd"
```

You can also leave the MAC address unspecified (just `--network name=localbr,mode=manual`). If you do so, Multipass will generate a random MAC for you, but you will need to retrieve it in the next step.

## Step 3: Configure the extra interface

We now need to configure the manual network interface inside the instance. We can achieve that using Netplan. The following command plants the required Netplan configuration file in the instance:

```
multipass exec -n test1 -- sudo bash -c 'cat << EOF > /etc/netplan/10-custom.yaml
network:
    version: 2
    ethernets:
        extra0:
            dhcp4: no
            match:
                macaddress: "52:54:00:4b:ab:cd"
            addresses: [10.13.31.13/24]
EOF'
```

The IP address needs to be unique and in the same subnet as the bridge. The MAC address needs to match the extra interface inside the instance: either the one provided in step 2 or the one Multipass generated (you can find it with `ip link`). 

If you want to set a different name for the interface, you can add a [`set-name` property](https://netplan.readthedocs.io/en/stable/netplan-yaml/#properties-for-physical-device-types).

## Step 4: Apply the new configuration

We now tell `netplan` apply the new configuration inside the instance:

```
multipass exec -n test1 -- sudo netplan apply
```

You may also use `netplan try`, to have the outcome reverted if something goes wrong.

## Step 5: Confirm that it works

You can confirm that the new IP is present in the instance with Multipass:

```
multipass info test1
```

The command above should show two IPs, the second of which is the one we just configured (`10.13.31.13`). You can use `ping` to confirm that it can be reached from the host:

```
ping 10.13.31.13
```

Conversely, you can also ping from the instance to the host:

```
multipass exec -n test1 -- ping 10.13.31.1
```

## Step 6: More instances

If desired, repeat steps 2-5 with different names/MACs/IP terminations (e.g. `10.13.31.14`) to launch other instances with static IPs in the same network. You can ping from one instance to another to confirm that they are connected. For example:

```
multipass exec -n test1 -- ping 10.13.31.14
``` 

## Conclusion

You have now created a small internal network, local to your host, with Multipass instances that you can reach on the same IP across reboots. Instances still have the default NAT-ed network, which they can use to reach the outside world. You can combine this with other networks if you want to (e.g. for bridging).

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/configure-static-ips" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

