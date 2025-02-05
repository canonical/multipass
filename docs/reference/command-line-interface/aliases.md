# aliases
> See also: [Alias](/explanation/alias), [How to use command aliases](/how-to-guides/manage-instances/use-instance-command-aliases)

The `aliases` command shows the aliases defined for all the instances. 

```plain
multipass aliases
```

The output will be similar to the following:

```plain
Alias  Instance         Command  Working directory
lsrm   rewarded-merlin  ls       default
topfp  flying-pig       top      map
```

The `Working directory` column tells us the directory of the host where the alias will be run. The value `default` means that the alias will be run  in the instance's default working directory (normally, `/home/ubuntu`). The value `map` means that, if the host directory from which the user calls the alias is mounted on the instance, the alias will be run on the mounted directory on the instance. 

[note type="information"]
The value will be `default` only if the alias was created with the `--no-map-working-directory` option.
```

The command can be used in conjunction with the `--format` or `-f` options to specify the desired output format: `csv`, `json`, `table` or `yaml`.

---

The full `multipass help aliases` output explains the available options:

```plain
Usage: multipass aliases [options]
List available aliases

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --format <format>  Output list in the requested format. Valid formats are:
                     table (default), json, csv and yaml. The output working
                     directory states whether the alias runs in the instance's
                     default directory or the alias running directory should try
                     to be mapped to a mounted one.
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/aliases-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

