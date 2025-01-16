(reference-settings-local-instance-name-disk)=
# local.\<instance-name\>.disk

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`launch`](/reference/command-line-interface/launch)

## Key

`local.<instance-name>.disk`

where `<instance-name>` is the name of a Multipass instance.

## Description

The size of the instance's disk.

```{caution}
The disk size can only be increased.
Decreasing the disk size is not permitted since it could cause data loss and the VM might break. 
```

## Allowed values

A size no smaller than the current disk size. This size can be specified with a positive integer or decimal number, optionally followed by a unit. Any case variations of the following suffixes are accepted as units:
  - B, to designate one byte.
  - KiB, KB, or K, to designate 1024 bytes.
  - MiB, MB, or M, to designate 1024 x 1024 = 1048576 bytes
  - GiB, GB, or G, to designate 1024 x 1024 x 1024 = 1073741824 bytes
  
```{note}
Decimal bytes (e.g. 1.1B) are refused, unless specified with larger units (>= KiB), in which case the amount is floored (e.g. 1.2KiB is interpreted as 1228B, even though 1.2 x 1024 = 1228.8).
```

## Examples

`multipass set local.handsome-ling.disk=7.5G`

## Default value

The size of the disk that the instance was launched with.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/local.<instance-name>.disk" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

