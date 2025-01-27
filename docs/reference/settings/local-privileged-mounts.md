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

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/privileged-mounts" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

