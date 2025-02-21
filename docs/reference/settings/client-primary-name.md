(reference-settings-client-primary-name)=
# client.primary-name

> See also: [Instance](/explanation/instance), [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`launch`](/reference/command-line-interface/launch)

## Key

`client.primary-name`

## Description

The name of the instance that is launched/recognised as primary.

> See also: {ref}`primary-instance`.

## Possible values

Any valid instance name or the empty string (`""`) to disable primary.

## Examples

- `multipass set client.primary-name=asdf`
- `multipass set client.primary-name=`

## Default value

`primary`
