(reference-settings-local-passphrase)=
# local.passphrase

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`authenticate`](/reference/command-line-interface/authenticate)

## Key

`local.passphrase`

## Description

Passphrase to authenticate users with the Multipass service, via the [`authenticate`](/reference/command-line-interface/authenticate) command.

The passphrase can be given directly in the command line; if omitted, the user will be prompted to enter it.

<!-- TODO: this prompts settings prompts should probably be documented separately -->

## Possible values

Any string

## Examples

`multipass set local.passphrase`

## Default value

Not set (expressed as `false` by `multipass get`)
