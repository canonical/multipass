(reference-command-line-interface-unalias)=
# unalias

The `multipass unalias` command removes a previously defined alias.

```{code-block} text
multipass unalias <name>
```

This will remove the given alias `<name>`, returning an error if the alias is not defined.

```{note}
If an instance is deleted, it is not necessary to run `unalias` for the aliases defined on that instance, as they are automatically removed.
```

You can remove multiple aliases in a single command:

```{code-block} text
multipass unalias <name1> <name2> <name3>
```

Or, use the argument `--all` to remove all the aliases in the current context:

```{terminal}
:prompt: ">"

multipass unalias --all

>
```

Aliases are grouped in contexts, and only one context is active at a time. The `--all` argument leaves the aliases in other contexts in place. To remove those too, switch to each context with [`prefer`](reference-command-line-interface-prefer) and run `multipass unalias --all` again.

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

```{seealso}
{ref}`How to use command aliases <how-to-guides-manage-instances-use-instance-command-aliases>`, [`alias`](reference-command-line-interface-alias), [`prefer`](reference-command-line-interface-prefer)
```
