(reference-command-line-interface-unalias)=
# unalias

> See also: [`alias`](/reference/command-line-interface/alias), [Alias](/explanation/alias), [How to use command aliases](/how-to-guides/manage-instances/use-instance-command-aliases)

The `multipass unalias` command removes a previously defined alias. 

```{code-block} text
multipass unalias name
```

This will remove the given alias `name`, returning an error if the alias is not defined. 

```{note}
If an instance is deleted and purged, it is not necessary to run `unalias` for the aliases defined on that instance, as they are automatically removed.
```

You can remove multiple aliases in a single command:

```{code-block} text
multipass unalias name1 name2 name3
```

Or, use the argument `--all` to remove all the defined aliases:

```{code-block} text
multipass unalias --all
```

---

The full `multipass help unalias` output explains the available options:

```{code-block} text
Usage: multipass unalias [options] <name> [<name> ...]
Remove aliases

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --all          Remove all aliases from current context

Arguments:
  name           Names of aliases to remove
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/unalias-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

