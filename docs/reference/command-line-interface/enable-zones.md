(reference-command-line-interface-enable-zones)=
# enable-zones

> See also: [Availability zone](/explanation/availability-zone), [`disable-zones`](/reference/command-line-interface/disable-zones), [`zones`](/reference/command-line-interface/zones)

The `multipass enable-zones` command makes one or more [availability zones](/explanation/availability-zone) available again after they were disabled with [`disable-zones`](/reference/command-line-interface/disable-zones). Any instances that were running in the zone when it was disabled are started back up.

Pass one or more zone names as arguments, or use `--all` to enable every zone at once:

```{code-block} text
multipass enable-zones zone2 zone3
```

```{code-block} text
Zones enabled: zone2, zone3
```

---

The full `multipass help enable-zones` output explains the available options:

```{code-block} text
Usage: multipass enable-zones [options] <zone> [<zone> ...]
Makes the given availability zones available. Instances therein are
started if they were running when their zone was last disabled.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short
                option for more detail. Maximum verbosity is obtained with
                4 (or more) v's, i.e. -vvvv.
  --all          Enable all zones

Arguments:
  zone           Name of the zones to make available
```
