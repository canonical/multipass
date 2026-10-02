(reference-command-line-interface-list)=
# list

> See also: {ref}`info <reference-command-line-interface-info>`, {ref}`launch <reference-command-line-interface-launch>`, {ref}`snapshot <reference-command-line-interface-snapshot>`, {ref}`Availability zone <explanation-availability-zone>`

The `multipass list` command lists available instances. It presents a generic view of instances, with some of their properties, including the {ref}`availability zone <explanation-availability-zone>` each instance is in; for example:

```{code-block} text
Name                    State             IPv4             Release            Zone
primary                 Suspended         --               Ubuntu 26.04 LTS   zone1
calm-squirrel           Running           10.218.69.109    Ubuntu 26.04 LTS   zone2
```

If an instance's zone has been disabled with [`multipass disable-zones`](reference-command-line-interface-disable-zones), `(n/a)` is appended to its zone in the table output, and its state shows as `Unavailable` (see {ref}`Instance states <reference-instance-states>`).

You can also use the `--format` option to get machine-readable output (CSV, JSON, or YAML), which also includes each instance's zone name and availability. For example, `multipass list --format yaml`:

```{code-block} text
primary:
  - state: Suspended
    zone:
      name: zone1
      available: true
    ipv4:
      - ""
    release: 26.04 LTS
calm-squirrel:
  - state: Running
    zone:
      name: zone2
      available: true
    ipv4:
      - 10.218.69.109
    release: 26.04 LTS
```

---
The full `multipass help list` output explains the available options:

```{code-block} text
Usage: multipass list [options]
List all instances which have been created.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --format <format>  Output list in the requested format.
                     Valid formats are: table (default), json, csv and yaml
```
