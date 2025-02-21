(reference-command-line-interface-delete)=
# delete

> See also: [`recover`](/reference/command-line-interface/recover), [`purge`](/reference/command-line-interface/purge)

The `multipass delete` command deletes the instances or snapshots that are specified as arguments.

You can provide multiple arguments in the same delete command, including both instances and snapshots; for example:

```{code-block} text
multipass delete --purge legal-takin calm-squirrel.snapshot2
```

Deleted instances are marked as such and removed from use, but you can still recover them using the `multipass recover` command, unless you used the `-p`/`--purge` option to delete them permanently.

To completely destroy instances and release the disk space they take up, use the `--purge` option or the [`multipass purge`](/reference/command-line-interface/purge) command.

```{caution}
When you delete a [snapshot](/explanation/snapshot), or when you delete an instance using the [GUI client](/reference/gui-client), Multipass removes them permanently (even if you didn't use the `--purge` option) and they cannot be recovered.

```

The `--all` option will delete all instances and their snapshots. Take care if using this option.

---

The output of `multipass help delete` explains the available options:

```{code-block} text
Usage: multipass delete [options] <instance>[.snapshot] [<instance>[.snapshot] ...]
Delete instances and snapshots. Instances can be purged immediately or later on,
with the "purge" command. Until they are purged, instances can be recovered
with the "recover" command. Snapshots cannot be recovered after deletion and must be purged at once.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --all          Delete all instances and snapshots
  -p, --purge    Permanently delete specified instances and snapshots
                 immediately

Arguments:
  name           Names of instances and snapshots to delete
```
