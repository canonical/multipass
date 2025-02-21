(reference-settings-local-instance-name-cpus)=
# local.\<instance-name\>.cpus

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [`launch`](/reference/command-line-interface/launch).

## Key

`local.<instance-name>.cpus`

where `<instance-name>` is the name of a Multipass instance.

## Description

The number of CPUs to simulate on the virtual machine. This establishes a limit on how many host threads the instance can simultaneously use, at most.

## Possible values

A positive integer number. Multipass does not impose an upper limit on the possible values, but the underlying backend may do so.

## Examples

`multipass set local.handsome-ling.cpus=4`

## Default value

The number of CPUs that the instance was launched with.
