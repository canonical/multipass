(reference-command-line-interface-suspend)=
# suspend

The `multipass suspend` command without any argument will suspend the {ref}`primary-instance` (and fail, if it doesn’t exist). You can also pass one or more instance names or the `--all` option to suspend more instances at the same time.

```{note}
Only instances in `Running` status can be suspended.
```

For example:

```{code-block} text
multipass stop boisterous-tortoise
multipass suspend boisterous-tortoise
```

If check your instances with `multipass list`, you'll see that its status is now set to `Suspended`:

```{code-block} text
Name                    State             IPv4             Image
boisterous-tortoise     Suspended         --               Ubuntu 22.04 LTS
```

Suspended instances can be resumed with the [`multipass start`](/reference/command-line-interface/start) command.

---

The full `multipass help suspend` output explains the available options:

```{code-block} text
Usage: multipass suspend [options] [<name> ...]
Suspend the named instances, if running. Exits with
return code 0 if successful.

Options:
  -h, --help     Display this help
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --all          Suspend all instances

Arguments:
  name           Names of instances to suspend. If omitted, and without the
                 --all option, 'primary' will be assumed.
```
