(reference-settings-local-privileged-mounts)=
# local.privileged-mounts

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`mount`](/reference/command-line-interface/mount), [Mount](/explanation/mount)

## Key

`local.privileged-mounts`

## Description

Controls whether [`multipass mount`](/reference/command-line-interface/mount) is allowed.

## Possible values

Any case variations of `on`|`off`, `yes`|`no`, `1`|`0` or `true`|`false`.

## Examples

`multipass set local.privileged-mounts=Yes`

## Default value

 - `true` on Linux and macOS
 - `false` on Windows
