(reference-settings-local-instance-name-snapshot-name-comment)=
# local.\<instance-name\>.\<snapshot-name\>.comment

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`snapshot`](/reference/command-line-interface/snapshot)

## Key

`local.<instance-name>.<snapshot-name>.comment`

Where `<instance-name>` is the name of a Multipass instance and `<snapshot-name>` is the current name of one of its snapshots.

## Description

An optional free piece of text that is associated with the snapshot. This can be used to describe the snapshot, recognise its role, or any other purpose.

## Possible values

Any string that your terminal can encode.

## Examples

`multipass set local.relative-lion.snaphot2.comment="Got it! 😏"`

## Default value

The comment that was assigned to the snapshot when it was taken.
