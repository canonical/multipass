(how-to-guides-manage-instances-use-availability-zones)=
# Use availability zones

> See also: [Availability zone](explanation-availability-zone), [`zones`](reference-command-line-interface-zones), [`enable-zones`](reference-command-line-interface-enable-zones), [`disable-zones`](reference-command-line-interface-disable-zones)

This document shows how to list the availability zones known to Multipass, launch an instance into a specific zone, and simulate the loss of a zone to see how your instances react.

## List the available zones

> See also: [`zones`](reference-command-line-interface-zones)

Run `multipass zones` to see the zones known to Multipass, along with their availability and subnet:

```{code-block} text
Name    State        Subnet
zone1   Available    10.42.0.0/24
zone2   Available    10.42.1.0/24
zone3   Available    10.42.2.0/24
```

## Launch an instance into a specific zone

> See also: [`launch --zone`](reference-command-line-interface-launch)

By default, `multipass launch` picks a zone for you automatically. To choose the zone yourself, pass the `--zone` option:

```{code-block} text
multipass launch --zone zone2 --name in-zone2
```

```{code-block} text
...
Launched: in-zone2 in zone2
```

`multipass info in-zone2` confirms the instance's zone:

```{code-block} text
Name:           in-zone2
State:          Running
Zone:           zone2
...
```

## Simulate a zone outage

> See also: [`disable-zones`](reference-command-line-interface-disable-zones), [Instance states](reference-instance-states)

To simulate a loss of availability, disable the zone with `multipass disable-zones`:

```{code-block} text
multipass disable-zones zone2
```

```{code-block} text
This operation will forcefully stop the VMs in zone2. Are you sure you want to continue? (Yes/no) yes
Zone disabled: zone2
```

Any instances in `zone2`, including `in-zone2`, are forcefully stopped and their state changes to `Unavailable`:

```{code-block} text
multipass list
```

```{code-block} text
Name         State         IPv4    Image             Zone
in-zone2     Unavailable   --      Ubuntu 26.04 LTS   zone2(n/a)
```

While the zone is disabled, you can't launch new instances into it, and existing instances in it can't be started, stopped, or otherwise interacted with.

## Re-enable the zone

> See also: [`enable-zones`](reference-command-line-interface-enable-zones)

Run `multipass enable-zones` to make the zone available again. Instances that were running when the zone was disabled, such as `in-zone2`, are started back up automatically:

```{code-block} text
multipass enable-zones zone2
```

```{code-block} text
Zone enabled: zone2
```
