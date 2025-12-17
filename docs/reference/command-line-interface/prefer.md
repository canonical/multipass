(reference-command-line-interface-prefer)=
# prefer

> See also:  [How to use command aliases](/how-to-guides/manage-instances/use-instance-command-aliases)

The `prefer` command, introduced in Multipass release 1.11.0,  is used to switch alias contexts. It accepts a single argument, specifying the name of the context to switch to.

For example, the command `multipass prefer secondary` makes `secondary` the current context. If the given context does not exist, Multipass creates it.

Subsequent calls to [`alias`](/reference/command-line-interface/alias) , [`unalias`](/reference/command-line-interface/unalias) and running any aliases from the command line will use the aliases defined in the given context.

---

The full `multipass help prefer` output explains the available options:

```{code-block} text
Usage: multipass prefer [options] <name>
Switch the current alias context. If it does not exist, create it before switching.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.

Arguments:
  name           Name of the context to switch to
```
