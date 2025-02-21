(reference-settings-local-instance-name-snapshot-name-name)=
# local.\<instance-name\>.\<snapshot-name\>.name

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`snapshot`](/reference/command-line-interface/snapshot), [Instance](/explanation/instance)

## Key

`local.<instance-name>.<snapshot-name>.name`

where `<instance-name>` is the name of a Multipass instance and `<snapshot-name>` is the current name of one of its snapshots.

## Description

The name that identifies the snapshot in the context of its instance. Snapshot names cannot be repeated.

## Possible values

Any name that follows the same rules as [instance names](/reference/instance-name-format) and is not currently in use by another instance.

## Examples

`multipass set local.relative-lion.snaphot2.name=primed`

## Default value

The name that was assigned to the snapshot when it was taken.
