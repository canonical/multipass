(reference-settings-local-dns-domain)=
# local.dns-domain

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set)

## Key

`local.dns-domain`

## Description

The DNS domain under which instances resolve from the host, as `<instance-name>.<domain>`. An instance resolves once it is running and reachable, and stops resolving when it is stopped, suspended or deleted.

An empty value disables name resolution.

```{note}
This setting currently only takes effect on Windows.
```

## Possible values

A DNS domain name, made of letters, digits and hyphens, with labels separated by dots. Names are case-insensitive. An empty value is also accepted.

## Examples

`multipass set local.dns-domain=mp.local`

With this, an instance named `primary` resolves as `primary.mp.local`.

## Default value

`multipass.local`
