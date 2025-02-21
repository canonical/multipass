(reference-settings-client-apps-windows-terminal-profiles)=
# client.apps.windows-terminal.profiles

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [How to integrate with Windows Terminal](/how-to-guides/customise-multipass/integrate-with-windows-terminal)

## Key

`client.apps.windows-terminal.profiles`

## Description

Which profiles should be enabled in Windows Terminal.
<!-- TODO: needs explanation -->

## Possible values

The following values are supported:

  - `primary` to enable a profile for the {ref}`primary-instance`. Note that this value is independent of what primary name is configured.
  - `none` to disable any profiles.

## Examples

- `multipass set client.apps.windows-terminal.profiles=none`

## Default value

`primary` (a profile for `primary` is added when Windows Terminal is found)
