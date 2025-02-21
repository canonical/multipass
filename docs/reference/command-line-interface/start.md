(reference-command-line-interface-start)=
# start

The `multipass start` command without any argument will start the {ref}`primary-instance` (and also create it, if it doesn't exist). You can also pass one or more instance names or the `--all` option to start more instances at the same time.

For example, the command `multipass start` will produce the following output:

```{code-block} text
Configuring primary \
Launching primary |
...
```

Only instances in `Stopped` or `Suspended` status can be started. Running instances can be restarted with [`multipass restart`](/reference/command-line-interface/restart), stopped with [`multipass stop`](/reference/command-line-interface/stop), and suspended with [`multipass suspend`](/reference/command-line-interface/suspend).

---

The full `multipass help start` output explains the available options:

```{code-block} text
Usage: multipass start [options] [<name> ...]
Start the named instances. Exits with return code 0
when the instances start, or with an error code if
any fail to start.

Options:
  -h, --help           Displays help on commandline options
  -v, --verbose        Increase logging verbosity. Repeat the 'v' in the short
                       option for more detail. Maximum verbosity is obtained
                       with 4 (or more) v's, i.e. -vvvv.
  --all                Start all instances
  --timeout <timeout>  Maximum time, in seconds, to wait for the command to
                       complete. Note that some background operations may
                       continue beyond that. By default, instance startup and
                       initialisation is limited to 5 minutes each.

Arguments:
  name                 Names of instances to start. If omitted, and without the
                       --all option, 'primary' (the configured primary instance
                       name) will be assumed. If 'primary' does not exist but is
                       included in a successful start command either implicitly
                       or explicitly), it is launched automatically (see
                       `launch` for more info).

```
