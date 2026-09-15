(reference-command-line-interface-zones)=
# zones

> See also: [Availability zone](/explanation/availability-zone), [`enable-zones`](/reference/command-line-interface/enable-zones), [`disable-zones`](/reference/command-line-interface/disable-zones), [`launch`](/reference/command-line-interface/launch)

The `multipass zones` command lists all the [availability zones](/explanation/availability-zone) known to Multipass, together with their current availability and the subnet reserved for instances launched into them.

For example:

```{code-block} text
Name    State        Subnet
zone1   Available    10.42.0.0/24
zone2   Available    10.42.1.0/24
zone3   Unavailable  10.42.2.0/24
```

A zone in the `Unavailable` state has been disabled with [`disable-zones`](/reference/command-line-interface/disable-zones); instances cannot be launched into it, and any instances already in it are forcefully stopped until the zone is re-enabled with [`enable-zones`](/reference/command-line-interface/enable-zones).

Like [`list`](/reference/command-line-interface/list), `zones` supports the `--format` option to get machine-readable output. For example, `multipass zones --format yaml`:

```{code-block} text
zone1:
  available: true
  subnet: 10.42.0.0/24
zone2:
  available: true
  subnet: 10.42.1.0/24
zone3:
  available: false
  subnet: 10.42.2.0/24
```

---

The full `multipass help zones` output explains the available options:

```{code-block} text
Usage: multipass zones [options]
List all availability zones, along with their availability status.

Options:
  -h, --help          Displays help on commandline options
  -v, --verbose       Increase logging verbosity. Repeat the 'v' in the short
                      option for more detail. Maximum verbosity is obtained with
                      4 (or more) v's, i.e. -vvvv.
  --format <format>   Output list in the requested format.
                      Valid formats are: table (default), json, csv and yaml
```
