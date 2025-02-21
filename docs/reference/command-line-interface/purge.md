(reference-command-line-interface-purge)=
# purge

> See also: [`multipass delete`](/reference/command-line-interface/delete), [`multipass recover`](/reference/command-line-interface/recover)

The `multipass purge` command will permanently remove all instances deleted with the `multipass delete` command. This will destroy all the traces of the instance, and cannot be undone.

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
