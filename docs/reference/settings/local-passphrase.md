(reference-settings-local-passphrase)=
# local.passphrase

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`authenticate`](/reference/command-line-interface/authenticate)

## Key

`local.passphrase`

## Description

Passphrase used by clients requesting access to the Multipass service via the [`multipass authenticate`](/reference/command-line-interface/authenticate) command.

The passphrase can be given directly in the command line; if omitted, the user will be prompted to enter it.

<!-- TODO: this prompts settings prompts should probably be documented separately -->

## Possible values

Any string

## Examples

`multipass set local.passphrase`

## Default value

Not set (expressed as `false` by `multipass get`)

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/passphrase" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
