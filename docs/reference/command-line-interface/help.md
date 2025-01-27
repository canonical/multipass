(reference-command-line-interface-help)=
# help

The `multipass help` command without an argument will list all the available commands with a brief explanation. It accepts a `<command>` argument to get detailed help for the command in question.

Sample output:

```{code-block} text
Usage: multipass [options] <command>
Create, control and connect to Ubuntu instances.

This is a command line utility for multipass, a
service that manages Ubuntu instances.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.

Available commands:
...
```

See our [CLI reference](/reference/command-line-interface/index) for information on the available commands.

The `--verbose` option will increase its output verbosity and you can repeat it (up to three times) to get even more output. By default, only errors will be displayed, with `-v` warnings would be added, `-vv` enables informational messages and `-vvv` will print out debugging information.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/help-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

