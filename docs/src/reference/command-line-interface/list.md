(reference-command-line-interface-list)=
# list

> See also: [`info`](/reference/command-line-interface/info), [`launch`](/reference/command-line-interface/launch), [`snapshot`](/reference/command-line-interface/snapshot)

The `multipass list` command lists available instances or snapshots. With no options, it presents a generic view of instances, with some of their properties; for example:

```{code-block} text
Name                    State             IPv4             Release
primary                 SUSPENDED         --               Ubuntu 18.04 LTS
calm-squirrel           RUNNING           10.218.69.109    Ubuntu 16.04 LTS
```

You can also call it with the `--snapshots` flag to get an overview of available snapshots. Here's a sample output of `multipass list --snapshots`:

```{code-block} text
Instance        Snapshot    Parent      Comment
calm-squirrel   snapshot1   --          --
calm-squirrel   snapshot3   snapshot1   Before restoring snapshot2
```

The `multipass list` command will truncate long snapshot comments, as well as those containing newlines. You can use [`info`](/reference/command-line-interface/info) to view them in full.

You can also use the `--format` option to get machine-readable output (CSV, JSON, or YAML). For example, `multipass list --format yaml`:

```{code-block} text
primary:
  - state: SUSPENDED
    ipv4:
      - ""
    release: 18.04 LTS
calm-squirrel:
  - state: RUNNING
    ipv4:
      - 10.218.69.109
    release: 16.04 LTS
```

---
The full `multipass help list` output explains the available options:

```{code-block} text
Usage: multipass list [options]
List all instances or snapshots which have been created.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --snapshots        List all available snapshots
  --format <format>  Output list in the requested format.
                     Valid formats are: table (default), json, csv and yaml
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/list-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

