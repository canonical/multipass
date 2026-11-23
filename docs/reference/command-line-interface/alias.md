(reference-command-line-interface-alias)=
# alias

> See also: [How to use command aliases](/how-to-guides/manage-instances/use-instance-command-aliases).

The `alias` command makes Multipass create a persistent alias to run commands on a given instance. Its syntax is the following:

```{code-block} text
multipass alias instance:command [name]
```

If `name` is omitted, the alias name defaults to `command`.

After running this command, a new alias is defined as running the `command` on the given instance.

By default, if the host folder where the alias is being executed is mounted on the instance, the instance's working directory is changed to the mounted directory. This behaviour can be avoided by defining the alias with the option `--no-map-working-directory`.

---

The full `multipass help alias` output explains the available options:

```{code-block} text
Usage: multipass alias [options] <definition> [<name>]
Create an alias to be executed on a given instance.

Options:
  -h, --help                      Displays help on commandline options
  -v, --verbose                   Increase logging verbosity. Repeat the 'v' in
                                  the short option for more detail. Maximum
                                  verbosity is obtained with 4 (or more) v's,
                                  i.e. -vvvv.
  -n, --no-map-working-directory  Do not automatically map the host execution
                                  path to a mounted path

Arguments:
  definition                      Alias definition in the form
                                  <instance>:<command>
  name                            Name given to the alias being defined,
                                  defaults to <command>
```
