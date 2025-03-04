(reference-command-line-interface-snapshot)=
# snapshot

> See also: [`restore`](/reference/command-line-interface/restore), [`list`](/reference/command-line-interface/list), [`info`](/reference/command-line-interface/info), [`delete`](/reference/command-line-interface/delete)

The `multipass snapshot` command takes a snapshot of an instance; for example:

```{code-block} text
multipass snapshot maximal-stag
```

The output will be similar to the following:

```{code-block} text
...
Snapshot taken: maximal-stag.snapshot1
```
The snapshot will record all the information that is required to later restore the instance to the same state. For the time being, the `snapshot` command can only operate on instances in `Stopped` status.

You have the option to specify a snapshot name using the `--name` option, following the same format as the [instance name format](/reference/instance-name-format).

If you don't specify a name, the snapshot will be named "snapshotN", where N is the total number of snapshots that were ever taken of that instance. You can also add an optional comment with `--comment`. Both of these properties can be later modified with the [`set`](/reference/command-line-interface/set) command, via the snapshot settings keys documented in [Settings](/reference/settings/index).

---

The full `multipass help snapshot` output explains the available options:

```{code-block} text
Usage: multipass snapshot [options] instance
Take a snapshot of an instance that can later be restored to recover the current state.

Options:
  -h, --help                   Displays help on commandline options
  -v, --verbose                Increase logging verbosity. Repeat the 'v' in
                               the short option for more detail. Maximum
                               verbosity is obtained with 4 (or more) v's, i.e.
                               -vvvv.
  -n, --name <name>            An optional name for the snapshot, subject to
                               the same validity rules as instance names (see
                               `help launch`). Default: "snapshotN", where N is
                               one plus the number of snapshots that were ever
                               taken for <instance>.
  --comment, -c, -m <comment>  An optional free comment to associate with the
                               snapshot. (Hint: quote the text to avoid spaces
                               being parsed by your shell)

Arguments:
  instance                     The instance to take a snapshot of.
```
