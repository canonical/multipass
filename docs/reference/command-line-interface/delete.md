(reference-command-line-interface-delete)=
# delete

> See also: [`recover`](/reference/command-line-interface/recover)

The `multipass delete` command deletes the instances or snapshots that are specified as arguments.

You can provide multiple arguments in the same delete command, including both instances and snapshots; for example:

```{code-block} text
multipass delete legal-takin calm-squirrel.snapshot2
Instance 'legal-takin' and snapshot calm-squirrel.snapshot2 will be deleted permanently. Would you like to proceed? [y/N]: y
legal-takin is deleted.
calm-squirrel.snapshot2 is deleted.
```

Use the `--force` option to skip the confirmation, for example in scripts. When Multipass cannot ask for confirmation (for example, when the command is not run in an interactive terminal), the command fails unless `--force` is given.

The `--all` option will delete all instances and their snapshots. Take care if using this option.

---

The output of `multipass help delete` explains the available options:

```{code-block} text
Usage: multipass delete [options] <instance>[.snapshot] [<instance>[.snapshot] ...]
Permanently delete instances and snapshots (in stopped instances).
Deleted instances and snapshots cannot be recovered after deletion.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --all          Delete all instances and snapshots
  --force        Do not ask for confirmation

Arguments:
  name           Names of instances and snapshots to delete
```
