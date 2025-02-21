(reference-settings-local-instance-name-bridged)=
# local.\<instance-name\>.bridged

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`local.bridged-network`](/reference/settings/local-bridged-network), [How to add networks to existing instances](/how-to-guides/manage-instances/add-a-network-to-an-existing-instance)

## Key

`local.<instance-name>.bridged`

where `<instance-name>` is the name of a Multipass instance.

## Description

Whether or not the virtual machine is connected to the preferred bridge that is currently defined by `local.<instance-name>.bridged`.

Setting this to `true` will cause the instance to be bridged to that host network interface.

Removing a bridged network from an instance is currently not supported.

## Possible values

This setting can have a boolean value (`true` or `false`). However, at this time, it can only be manually set to `true`, but not to `false`.

The value of this setting depends on the value of [`local.bridged-network`](/reference/settings/local-bridged-network); that is, it varies dynamically according to the configured preferred network and the networks that have been added to the instance so far.

## Examples

`multipass set local.ultimate-grosbeak.bridged=true`

If the instance `ultimate-grosbeak` was launched with the command `multipass launch --network eth0`, the result of `multipass get local.ultimate-grosbeak.bridged` will be `true` for as long as the value of `local.bridged-network` is `eth0`.

If you run the command `multipass set local.bridged-network=eth1`, the result of `multipass get local.ultimate-grosbeak.bridged` will become `false`. At that point, you could run the command `multipass set local.ultimate-grosbeak.bridged=true` to bridge `ultimate-grosbeak` with `eth1`.

### Bridged connection to a physical network interface

On some platforms/backends, Multipass cannot connect instances directly to physical network interface controllers (NICs). In this case, Multipass offers to create a bridge/switch on the host connecting to that NIC. The instance is then connected to the bridge/switch, achieving the same effect as if it had been connected to the physical NIC directly. An instance that is indirectly connected with a physical NIC in this fashion is also considered to be bridged with that NIC by Multipass.

For example, if the preferred network is `eth1` and the instance `ultimate-grosbeak` is connected with a bridge `br-eth1` that is linked with `eth1`, then `multipass get local.ultimate-grosbeak.bridged` will return `true`.

## Default value

`true` if the instance is bridged with the preferred network, `false` otherwise.
