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

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/winterm-profiles" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

