(reference-command-line-interface-index)=
# Command-line interface

> See also: [Instance](/explanation/instance), [Service](/explanation/service)

The **`multipass`** CLI (command line interface) client is used to communicate with the Multipass service to create, manage, and interact with Multipass instances using various sub-commands.

You can use `multipass help <command>` to display more information on the purpose and available options of each command.

## Instance creation and removal

Every instance you work with starts with a launch or a clone. Removing an instance takes two steps: a deleted instance can still be recovered until you purge it.

- [`launch`](reference-command-line-interface-launch)
- [`clone`](reference-command-line-interface-clone)
- [`delete`](reference-command-line-interface-delete)
- [`recover`](reference-command-line-interface-recover)
- [`purge`](reference-command-line-interface-purge)

## Discovery and inspection

These commands report information without changing anything. Use them to choose an image before you launch, or to check an instance's IP address, disk usage, and memory usage.

- [`find`](reference-command-line-interface-find)
- [`list`](reference-command-line-interface-list)
- [`info`](reference-command-line-interface-info)
- [`networks`](reference-command-line-interface-networks)

## Instance state

A stopped instance does not consume resources, while a suspended instance keeps its state and memory so it can resume where it left off. See [Instance states](reference-instance-states) for the full list of states.

- [`start`](reference-command-line-interface-start)
- [`stop`](reference-command-line-interface-stop)
- [`suspend`](reference-command-line-interface-suspend)
- [`restart`](reference-command-line-interface-restart)

## Snapshots

A snapshot records an instance at a point in time, so you can experiment and roll back if something goes wrong. The instance must be stopped to take or restore a snapshot. See [Snapshot](explanation-snapshot) for what a snapshot records.

- [`snapshot`](reference-command-line-interface-snapshot)
- [`restore`](reference-command-line-interface-restore)

## Command-line access

You can work inside an instance interactively or run one command at a time from a script. See [Multipass exec and shells](explanation-multipass-exec-and-shells) for how your host shell affects the commands you pass.

- [`shell`](reference-command-line-interface-shell)
- [`exec`](reference-command-line-interface-exec)

## File sharing

A mount shares a host folder with an instance, while a transfer copies files between the host and an instance without mounting anything. See [Mount](explanation-mount) for the different types of mount.

- [`mount`](reference-command-line-interface-mount)
- [`umount`](reference-command-line-interface-umount)
- [`transfer`](reference-command-line-interface-transfer)

## Aliases

An alias lets you run a command inside an instance as if it were a command on your host. Aliases are grouped into contexts, so you can keep separate sets and switch between them. See [Alias](explanation-alias) for more.

- [`alias`](reference-command-line-interface-alias)
- [`aliases`](reference-command-line-interface-aliases)
- [`unalias`](reference-command-line-interface-unalias)
- [`prefer`](reference-command-line-interface-prefer)

## Availability zones

Availability zones group your instances the way a public cloud does, so you can simulate the loss of part of your infrastructure without affecting instances in other zones. See [Availability zone](explanation-availability-zone) for how instances are assigned to zones.

- [`zones`](reference-command-line-interface-zones)
- [`enable-zones`](reference-command-line-interface-enable-zones)
- [`disable-zones`](reference-command-line-interface-disable-zones)

## Configuration

Settings control how Multipass behaves, from the driver it uses to the resources assigned to each instance. See [Settings](reference-settings-index) for the full list of keys.

- [`get`](reference-command-line-interface-get)
- [`set`](reference-command-line-interface-set)

## Service and client

These commands work with the Multipass service and client as a whole, rather than with a specific instance. Use them to give a user access to the service, to wait for the service to be ready in a script, or to get help and version details.

- [`authenticate`](reference-command-line-interface-authenticate)
- [`wait-ready`](reference-command-line-interface-wait-ready)
- [`help`](reference-command-line-interface-help)
- [`version`](reference-command-line-interface-version)

```{toctree}
:hidden:
:titlesonly:
:maxdepth: 2
:glob:

*
```
