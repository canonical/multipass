(reference-command-line-interface-stop)=
# stop

The `multipass stop` command without any argument will stop the {ref}`primary-instance`
(and fail, if it doesn't exist). You can also pass one or more instance names or the `--all` option to stop more instances at the same time.

```{note}
Only instances in `Running` status can be stopped.
```

`Stopped` instances can be started again with [`multipass start`](/reference/command-line-interface/start).

You can use the `--force` option (*available starting from Multipass version 1.14*) to stop the instance when it is in a non-responsive, unknown or suspended state. It can also be used with the `--all` option to stop all instances forcefully.

---

The full `multipass help stop` output explains the available options:

```{code-block} text
Usage: multipass stop [options] [<name> ...]
Stop the named instances. Exits with return code 0
if successful.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --all              Stop all instances
  -t, --time <time>  Time from now, in minutes, to delay shutdown of the
                     instance
  -c, --cancel       Cancel a pending delayed shutdown
  --force            Force the instance to shut down immediately. Warning: This
                     could potentially corrupt a running instance, so use with
                     caution.

Arguments:
  name               Names of instances to stop. If omitted, and without the
                     --all option, 'primary' will be assumed.

```
