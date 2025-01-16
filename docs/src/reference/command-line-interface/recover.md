(reference-command-line-interface-recover)=
# recover

> See also: [`multipass delete`](/reference/command-line-interface/delete), [`multipass purge`](/reference/command-line-interface/purge)

The `multipass recover` command will revive an instance that was previously removed with `multipass delete`. For this to be possible, the instance cannot have been purged with `multipass purge` nor with `multipass delete --purge`. 

Use the `--all` option to recover all deleted instances at once:

```plain
multipass recover --all
```

---

The full `multipass help restart` output explains the available options:

```plain
Usage: multipass recover [options] <name> [<name> ...]
Recover deleted instances so they can be used again.

Options:
  -h, --help     Display this help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --all          Recover all deleted instances

Arguments:
  name           Names of instances to recover
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/recover-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

