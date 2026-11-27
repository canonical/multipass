(reference-command-line-interface-recover)=
# recover

```{caution}
The `recover` command is deprecated and will be removed in an upcoming release. When removed, all `DELETED` instances will be automatically recovered.
```

The `multipass recover` command will revive an instance that was marked as deleted by an earlier version of Multipass. Instances deleted with the current `multipass delete` are removed permanently and cannot be recovered.

Use the `--all` option to recover all deleted instances at once:

```{terminal}
:prompt: ">"

multipass recover --all

>
```

---

The full `multipass help recover` output explains the available options:

```{code-block} text
Usage: multipass recover [options] <name> [<name> ...]
Recover deleted instances so they can be used again.

Options:
  -h, --help     Display this help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --all          Recover all deleted instances

Arguments:
  name           Names of instances to recover
```

```{seealso}
{ref}`How to remove an instance <how-to-guides-manage-instances-remove-an-instance>`, [`delete`](/reference/command-line-interface/delete)
```
