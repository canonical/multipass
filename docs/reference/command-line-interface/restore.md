(reference-command-line-interface-restore)=
# restore

> See also: [`snapshot`](/reference/command-line-interface/snapshot), [`list`](/reference/command-line-interface/list), [`info`](/reference/command-line-interface/info), [`delete`](/reference/command-line-interface/delete)

The `multipass restore` command restores an instance to the state that it was in when the given snapshot was taken.

For example, when you run the command:

```{code-block} text
multipass restore relative-lion.snapshot2
```

the system will ask you if you want to save a snapshot of your instance before proceeding:

```{code-block} text
Do you want to take a snapshot of relative-lion before discarding its current state? (Yes/no):
```

If you confirm, the output will be similar to the following:

```{code-block} text
Snapshot taken: relative-lion.snapshot3
Snapshot restored: relative-lion.snapshot2
```

As shown in the example, with no further options, the command will offer to take another snapshot. This automatic snapshot saves the instance's current state before it is thrown away. It will be named following the [`multipass snapshot`](/reference/command-line-interface/snapshot) default naming convention, and it will have an automatic comment to indicate its purpose.

In our example, if you run:

```{code-block} text
multipass info relative-lion.snapshot3 | grep Comment
```

you'll find the comment:

```{code-block} text
Comment:        Before restoring snapshot1
```

You can use the `--destructive` (or `-d`)  option to skip the question and discard the current state. If the command is run non-interactively (i.e. with either standard input or standard output being redirected), this flag is required, since there is no way to query the user for confirmation.

---

The full `multipass help restore` output explains the available options:

```{code-block} text
Usage: multipass restore [options] <instance>.<snapshot>
Restore an instance to the state of a previously taken snapshot.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  -d, --destructive  Discard the current state of the instance

Arguments:
  instance.snapshot  The instance to restore and snapshot to use, in
                     <instance>.<snapshot> format, where <instance> is the name
                     of an instance, and <snapshot> is the name of a snapshot
```
