(reference-settings-local-instance-name-snapshot-name-comment)=
# local.\<instance-name\>.\<snapshot-name\>.comment

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`snapshot`](/reference/command-line-interface/snapshot)

## Key

`local.<instance-name>.<snapshot-name>.comment`

Where `<instance-name>` is the name of a Multipass instance and `<snapshot-name>` is the current name of one of its snapshots.

## Description

An optional free piece of text that is associated with the snapshot. This can be used to describe the snapshot, recognize its role, or any other purpose.

## Possible values

Any string that your terminal can encode.

## Examples

`multipass set local.relative-lion.snaphot2.comment="Got it! 😏"`

## Default value

The comment that was assigned to the snapshot when it was taken.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/snapshot-comment" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

