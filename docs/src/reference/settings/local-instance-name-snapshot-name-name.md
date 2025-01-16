(reference-settings-local-instance-name-snapshot-name-name)=
# local.\<instance-name\>.\<snapshot-name\>.name

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`snapshot`](/reference/command-line-interface/snapshot), [Instance](/explanation/instance)

## Key

`local.<instance-name>.<snapshot-name>.name`

where `<instance-name>` is the name of a Multipass instance and `<snapshot-name>` is the current name of one of its snapshots.

## Description

The name that identifies the snapshot in the context of its instance. Snapshot names cannot be repeated.

## Possible values

Any name that follows the same rules as [instance names](/t/28469#heading--Instance-name-format) and is not currently in use by another instance.

## Examples

`multipass set local.relative-lion.snaphot2.name=primed`

## Default value 

The name that was assigned to the snapshot when it was taken.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/snapshot-name" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

