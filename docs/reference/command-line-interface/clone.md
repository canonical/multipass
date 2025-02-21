(reference-command-line-interface-clone)=
# clone

The `multipass clone` command creates a clone of an instance. A cloned instance is a standalone instance that is a replica of the original instance in terms of its configuration, installed software, and data at the time of cloning. Cloning an instance can be useful for backup or testing purposes, or to create identical VMs from a working template.

```{caution}
The `clone` command is available since Multipass version 1.15.0.
```

Currently, only instances that are in the `Stopped` state can be cloned.

You can run the `clone` command  on a source instance without any additional options. For example, `multipass clone natty-nilgai` will produce the following output:

```{code-block} text
...
Cloned from natty-nilgai to natty-nilgai-clone1.
```

By default, the `multipass clone` command automatically generates a name for the cloned instance using the format *<source_vm_name>-cloneN*, where *N* represents the number of instances cloned from that specific source instance, starting at 1. In the example, the name of the source VM is "natty-nilgai" and the automatically generated name for its clone is "natty-nilgai-clone1".

Alternatively, you can specify a custom name for the cloned instance using `--name` option, following the [standard instance name format](/reference/instance-name-format). For example:

```
multipass clone natty-nilgai --name custom-clone
```

---

The full `multipass help clone` output explains the available options:

```{code-block} text
Usage: multipass clone [options] <source_name>
Create an independent copy of an existing (stopped) instance.

Options:
  -h, --help                     Displays help on commandline options
  -v, --verbose                  Increase logging verbosity. Repeat the 'v' in
                                 the short option for more detail. Maximum
                                 verbosity is obtained with 4 (or more) v's,
                                 i.e. -vvvv.
  -n, --name <destination-name>  An optional custom name for the cloned
                                 instance. The name must follow the usual
                                 validity rules (see "help launch"). Default:
                                 "<source_name>-cloneN", where N is the Nth
                                 cloned instance.

Arguments:
  source_name                    The name of the source instance to be cloned
```
