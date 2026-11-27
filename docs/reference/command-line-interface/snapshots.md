(reference-command-line-interface-snapshots)=
# snapshots

```{caution}
Snapshots are not supported on the Apple Virtualization framework driver, so this command finds no snapshots on that driver. See {ref}`driver-feature-disparities`.
```

The `multipass snapshots` command lists the available snapshots. Here's a sample output:

```{terminal}
:prompt: ">"

multipass snapshots

Instance        Snapshot    Parent      Comment
calm-squirrel   snapshot1   --          --
calm-squirrel   snapshot3   snapshot1   Before restoring snapshot2
```

The `multipass snapshots` command will truncate long snapshot comments, as well as those containing newlines. You can use [`info`](reference-command-line-interface-info) to view them in full.

You can also use the `--format` option to get machine-readable output (CSV, JSON, or YAML). For example, `multipass snapshots --format yaml`:

```{terminal}
:prompt: ">"

multipass snapshots --format yaml

calm-squirrel:
  - snapshot: snapshot1
      - parent: ~
        comment: ~
  - snapshot: snapshot3
      - parent: snapshot1
        comment: Before restoring snapshot2
```

---
The full `multipass help snapshots` output explains the available options:

```{code-block} text
Usage: multipass snapshots [options]
List all snapshots which have been created.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --format <format>  Output list in the requested format.
                     Valid formats are: table (default), json, csv and yaml
```

```{seealso}
{ref}`Snapshot <explanation-snapshot>`, [`snapshot`](reference-command-line-interface-snapshot), [`restore`](reference-command-line-interface-restore), [`info`](reference-command-line-interface-info), [`delete`](reference-command-line-interface-delete)
```
