(reference-settings-local-instance-name-memory)=
# local.\<instance-name\>.memory

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`launch`](/reference/command-line-interface/launch)

## Key

`local.<instance-name>.memory`

where `<instance-name>` is the name of a Multipass instance.

## Description

The amount of RAM that is allocated to the instance.

```{note}
Hypervisors may impose additional rounding on the total memory that is given to the instance. Furthermore, the value that is set here does not correspond exactly to the memory size that is available in userspace inside the instance (e.g. reported by `free -b`), because the guest kernel claims some for its own. Total memory can be inspected inside the instance with `lshw` (e.g. `sudo lshw -json -class memory`).
```

```{note}
Memory on the host is only allocated as the instance uses it, not right away. Once used, it is not released until the instance is shutdown or restarted.
```

## Allowed values

A positive memory size, specified with a positive integer or decimal number, optionally followed by a unit. Any case variations of the following suffixes are accepted as units:
  - B, to designate one byte
  - KiB, KB, or K, to designate 1024 bytes
  - MiB, MB, or M, to designate 1024 x 1024 = 1048576 bytes
  - GiB, GB, or G, to designate 1024 x 1024 x 1024 = 1073741824 bytes

```{note}
Decimal bytes (e.g. 1.1B) are refused, unless specified with larger units (>= KiB), in which case the amount is floored (e.g. 1.2KiB is interpreted as 1228B, even though 1.2 x 1024 = 1228.8).
```

## Examples

`multipass set local.handsome-ling.memory=4G`

## Default value

The amount of memory that the instance was launched with.
