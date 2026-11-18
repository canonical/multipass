(reference-command-line-interface-purge)=
# purge

```{caution}
The `purge` command is deprecated and will be removed in an upcoming release. Use `multipass delete <instance>` instead.
```

The `multipass purge` command will permanently remove all instances that are in the `Deleted` state. This will destroy all the traces of the instance, and cannot be undone.

For example:

```{terminal}
:prompt: ">"

multipass purge

*** Warning: `multipass purge` is deprecated and will be removed in an upcoming release. ***

Use `multipass delete <instance>` to permanently delete an instance.
```

---

The full `multipass help purge` output explains the available options:

```{code-block} text
Usage: multipass purge [options]
Purge all deleted instances permanently, including all their data.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
```

```{seealso}
{ref}`How to remove an instance <how-to-guides-manage-instances-remove-an-instance>`, [`delete`](/reference/command-line-interface/delete), [`recover`](/reference/command-line-interface/recover)
```
