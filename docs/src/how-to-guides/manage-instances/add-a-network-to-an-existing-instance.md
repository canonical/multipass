(how-to-guides-manage-instances-add-a-network-to-an-existing-instance)=
# Add a network to an existing instance

> See also: [`networks`](/reference/command-line-interface/networks), [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`local.<instance-name>.bridged`](/reference/settings/local-instance-name-bridged)

This guide explains how to bridge an existing Multipass instance with the available networks.

```{caution}
This feature is available starting from Multipass version 1.14.
```

First, you need to select a Multipass-wide preferred network to bridge with (you can always change it later). To do so, list all available networks using the [`multipass networks`](/reference/command-line-interface/networks) command. The output will be similar to the following:

```plain
Name      Type       Description
br-eth0   bridge     Network bridge with eth0
br-eth1   bridge     Network bridge with eth1
eth0      ethernet   Ethernet device
eth1      ethernet   Ethernet device
lxdbr0    bridge     Network bridge
mpbr0     bridge     Network bridge for Multipass
virbr0    bridge     Network bridge
```

Set the preferred network (for example, `eth0`) using the [`multipass set`](/reference/command-line-interface/set) command: 
  
```plain
multipass set local.bridged-network=eth0
```

Before bridging the network, you need to stop the instance (called `ultimate-grosbeak` in our example) using the [`multipass stop`](/reference/command-line-interface/stop) command:

```plain
multipass stop ultimate-grosbeak
```

You can now ask Multipass to bridge your preferred network using the [`local.<instance-name>.bridged`](/reference/settings/local-instance-name-bridged) setting:

```plain
multipass set local.ultimate-grosbeak.bridged=true
```

To add further networks, update the preferred bridge and repeat:

```plain
multipass set local.bridged-network=eth1
multipass set local.ultimate-grosbeak.bridged=true
```

Use the [`multipass get`](/reference/command-line-interface/get) command to check whether an instance is bridged with the currently configured preferred network:

```plain
multipass get local.ultimate-grosbeak.bridged
```

After following the recipe above, the result should be `true`.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/id-mapping" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

