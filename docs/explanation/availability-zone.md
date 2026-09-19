(explanation-availability-zone)=
# Availability zone

> See also: [`zones`](/reference/command-line-interface/zones), [`enable-zones`](/reference/command-line-interface/enable-zones), [`disable-zones`](/reference/command-line-interface/disable-zones), [`launch`](/reference/command-line-interface/launch), [Instance](/explanation/instance)

An **availability zone** (AZ) is a logical grouping of Multipass instances, modelled after the availability zones of public cloud providers. Zones let you spread instances out and simulate the loss of part of your "infrastructure", without affecting instances in other zones.

Every instance belongs to exactly one zone, chosen at launch time and visible as the `Zone` field in [`info`](/reference/command-line-interface/info) and [`list`](/reference/command-line-interface/list) output.

## Default zones

Out of the box, Multipass creates three zones: `zone1`, `zone2` and `zone3`. Each zone reserves its own subnet, so instances in different zones never share the same private network. Use [`zones`](/reference/command-line-interface/zones) to see the current zones, their availability, and their subnets.

## Choosing a zone at launch

By default, [`launch`](/reference/command-line-interface/launch) assigns the new instance to a zone automatically: Multipass cycles through the available zones, so consecutive launches without an explicit zone tend to land in different zones. Use the `--zone` option to pin the instance to a specific zone instead:

```{code-block} text
multipass launch --zone zone2
```

## Simulating an outage

[`disable-zones`](/reference/command-line-interface/disable-zones) makes one or more zones unavailable, simulating a loss of availability on a cloud provider:

- Any running instances in the affected zones are forcefully switched off, and their state is reported as `Unavailable` (see [Instance states](/reference/instance-states)).
- Multipass refuses to launch new instances into a disabled zone, and refuses to start or otherwise interact with the instances already there, until the zone is re-enabled.

[`enable-zones`](/reference/command-line-interface/enable-zones) reverses this: the zone becomes available again, and any instances that were running when the zone was disabled are started back up.

This makes it possible to test how a multi-instance deployment behaves when part of it becomes unreachable, without actually deleting or reconfiguring anything.
