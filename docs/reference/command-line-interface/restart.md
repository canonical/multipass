(reference-command-line-interface-restart)=
# restart

The `multipass restart` command without any argument will restart the {ref}`primary-instance` (and fail, if it doesn't exist). You can also pass one or more instance names or the `--all` option to restart more instances at the same time.

```{note}
Only instances in `Running` status can be restarted.
```

For example:

```{code-block} text
multipass restart desirable-earwig
```

---

The full `multipass help restart` output explains the available options:

```{code-block} text
Usage: multipass restart [options] [<name> ...]
Restart the named instances. Exits with return
code 0 when the instances restart, or with an
error code if any fail to restart.

Options:
  -h, --help           Displays help on commandline options
  -v, --verbose        Increase logging verbosity. Repeat the 'v' in the short
                       option for more detail. Maximum verbosity is obtained
                       with 4 (or more) v's, i.e. -vvvv.
  --all                Restart all instances
  --timeout <timeout>  Maximum time, in seconds, to wait for the command to
                       complete. Note that some background operations may
                       continue beyond that. By default, instance startup and
                       initialisation is limited to 5 minutes each.

Arguments:
  name                 Names of instances to restart. If omitted, and without
                       the --all option, 'primary' will be assumed.
```
