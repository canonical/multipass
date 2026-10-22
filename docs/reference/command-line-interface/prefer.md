(reference-command-line-interface-prefer)=
# prefer

> See also:  [How to use command aliases](/how-to-guides/manage-instances/use-instance-command-aliases)

```{caution}
The `prefer` command is deprecated and will be removed in an upcoming release. Use unique alias names to avoid switching alias contexts.
```

The `prefer` command previously switched alias contexts. It remains recognised during the deprecation period, but returns an error instead of creating or switching contexts.

Only the `default` context is active. Its aliases continue to work. Existing non-default contexts are inactive: their definitions remain visible in [`aliases`](/reference/command-line-interface/aliases), but cannot be executed, even using a context-qualified name.

To use a definition from an inactive context, recreate it in `default` with [`alias`](/reference/command-line-interface/alias), choosing a unique name. See [Use instance command aliases](/how-to-guides/manage-instances/use-instance-command-aliases) for the migration steps.

---

The full `multipass help prefer` output explains the available options:

```{code-block} text
Usage: multipass prefer [options] <name>
Alias contexts can no longer be switched. Only the default context is active. Use
'multipass aliases' to inspect inactive definitions.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.

Arguments:
  name           Name of the context (no longer supported)
```
