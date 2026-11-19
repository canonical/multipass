(reference-settings-local-passphrase)=
# local.passphrase

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`authenticate`](/reference/command-line-interface/authenticate)

## Key

`local.passphrase`

## Description

Passphrase used to authenticate users with the Multipass service via the [`authenticate`](/reference/command-line-interface/authenticate) command.

An authenticated user sets this value for other users to reuse. It can be given directly in the command line, or left unset to prompt for a hidden interactive entry.

<!-- TODO: this prompts settings prompts should probably be documented separately -->

## Possible values

Any string

## Examples

- `multipass set local.passphrase`
- `multipass set local.passphrase=foo`

## Default value

Not set (expressed as `false` by `multipass get`)
