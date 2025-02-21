(reference-settings-client-gui-autostart)=
# client.gui.autostart

```{caution}
This setting has been removed from the CLI starting from Multipass version 1.14.
It is now only available in the [GUI client](/reference/gui-client).
```

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [GUI client](/reference/gui-client)

## Key

`client.gui.autostart`

## Description

Whether or not the Multipass GUI should start automatically on startup.

## Possible values

Any case variations of `on`|`off`, `yes`|`no`, `1`|`0` or `true`|`false`.

## Examples

`multipass set client.gui.autostart=true`

## Default value

`true` (on Linux and macOS it only takes effect after the client (CLI or GUI) is run for the first time)
