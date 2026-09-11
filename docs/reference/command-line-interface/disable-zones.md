(reference-command-line-interface-disable-zones)=
# disable-zones

> See also: [Availability zone](/explanation/availability-zone), [`enable-zones`](/reference/command-line-interface/enable-zones), [`zones`](/reference/command-line-interface/zones)

The `multipass disable-zones` command makes one or more [availability zones](/explanation/availability-zone) unavailable, simulating a loss of availability on a cloud provider. Instances in the affected zones are forcefully switched off, and Multipass refuses to launch new instances into the zone, until it is re-enabled with [`enable-zones`](/reference/command-line-interface/enable-zones).

Pass one or more zone names as arguments, or use `--all` to disable every zone at once:

```{code-block} text
multipass disable-zones zone2
```

Since this forcefully stops any running instances in the zone, Multipass asks for confirmation before proceeding:

```{code-block} text
This operation will forcefully stop the VMs in zone2. Are you sure you want to continue? (Yes/no)
```

```{code-block} text
Zone disabled: zone2
```

Use the `--force` option to skip the confirmation prompt. This is required if the command is run non-interactively (i.e. with either standard input or standard output being redirected), since there is no way to query the user for confirmation in that case.

---

The full `multipass help disable-zones` output explains the available options:

```{code-block} text
Usage: multipass disable-zones [options] <zone> [<zone> ...]
Makes the given availability zones unavailable. Instances therein are
forcefully switched off and remain unavailable until their zone is
re-enabled (simulating a loss of availability on a cloud provider).

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short
                option for more detail. Maximum verbosity is obtained with
                4 (or more) v's, i.e. -vvvv.
  --all          Disable all zones
  --force        Do not ask for confirmation

Arguments:
  zone           Name of the zones to make unavailable
```
