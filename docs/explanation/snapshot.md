(explanation-snapshot)=
# Snapshot

A snapshot is an conceptual image of an instance at some point in time, that can be used to restore the instance to what it was at that instant.

To achieve this, a snapshot records all mutable properties of an instance, that is, all the properties that can change through interaction with Multipass. These include disk contents and size, number of CPUs, amount of memory, and mounts. On the other hand, aliases are not considered part of the instance and are not recorded.

## Usage

You can take a snapshot of an instance with the [`snapshot`](/reference/command-line-interface/snapshot) command, and restore it with the [`restore`](/reference/command-line-interface/restore) command. Taking and restoring a snapshot requires the instance to be stopped.

You can view a list of the available snapshots with `multipass list --snapshots` and the details of a particular snapshot with `multipass info <instance>.<snapshot>`. To delete a snapshot, use the [`multipass delete`](/reference/command-line-interface/delete) command.

> See also: [`list`](/reference/command-line-interface/list), [`info`](/reference/command-line-interface/info)

## Parents

An instance's disk contents are recorded by snapshots in layers: each new snapshot records changes with respect to its parent snapshot. A snapshot's parent is the snapshot that was last taken or restored when the new snapshot is taken. Parent and children snapshots of a deleted snapshot retain a consistent record of the instance. Multipass provides information of snapshots' parent/child relations to help identify their role or contents.

```{caution}
**Caveats:**
- Long chains of snapshots have a detrimental effect on performance. Since they rely on layers of disk diffs, the more snapshots there are in a sequence, the more hops are necessary to read data that is recorded by the most ancient layers.
- While snapshots are useful to save and recover instance states, their utility as safe backups is limited. Since they are stored in the same medium as the original images, they are as likely to be affected by disk failures.
```
