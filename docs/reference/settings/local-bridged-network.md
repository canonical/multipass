(reference-settings-local-bridged-network)=
# local.bridged-network

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`launch`](/reference/command-line-interface/launch), [`networks`](/reference/command-line-interface/networks), [Create an instance with multiple network interfaces](/t/27188#create-an-instance-with-multiple-network-interfaces), [Add a network to an existing instance](/how-to-guides/manage-instances/add-a-network-to-an-existing-instance), [`local.<instance-name>.bridged`](/reference/settings/local-instance-name-bridged)

## Key

`local.bridged-network`

## Description

The name of the interface to connect the instance to when the shortcut `launch --bridged` is issued. 

## Possible values

Any name from [`multipass networks`](/reference/command-line-interface/networks). 

Validation is deferred to [`multipass launch`](/reference/command-line-interface/launch).

## Examples

`multipass set local.bridged-network=en0`

## Default value

`<empty>` (`""`).

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/bridged-network" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

