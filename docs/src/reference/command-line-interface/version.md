# version
The `multipass version` command without an argument will display the client and daemon versions of Multipass; for example:

```plain
multipass  1.0.0
multipassd 1.0.0
```

If there is an update to Multipass available, it will be printed out in addition to the standard output; for example:

```plain
multipass  1.0.0
multipassd 1.0.0

########################################################################################
Multipass 1.0.1 release
Bugfix release to address a crash

Go here for more information: https://github.com/canonical/multipass/releases/tag/v1.0.1
########################################################################################
```

---

The full `multipass help version` output explains the available options:

```plain
Usage: multipass version [options]
Display version information about the multipass command
and daemon.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --format <format>  Output version information in the requested format.
                     Valid formats are: table (default), json, csv and yaml
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/version-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

