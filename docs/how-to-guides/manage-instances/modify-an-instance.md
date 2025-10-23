(how-to-guides-manage-instances-modify-an-instance)=
# Modify an instance

> See also: [Instance](explanation-instance), [`launch`](reference-command-line-interface-launch), [`set`](reference-command-line-interface-set), [Settings](reference-settings-index)

This document shows further ways to customise an instance outside of the [`launch`](reference-command-line-interface-launch) command using the Multipass [settings](reference-settings-index).

(set-the-cpu-ram-or-disk-of-an-instance)=
## Set the CPUs , RAM or disk space of an instance

> See also:  [`local.<instance-name>.cpus`](reference-settings-local-instance-name-cpus), [`local.<instance-name>.disk)`](reference-settings-local-instance-name-disk), [`local.<instance-name>.memory`](reference-settings-local-instance-name-memory)

You can set instance properties at `launch` time, but you can also update some of them after the instance has been created. Specifically, an instance’s memory, disk space, and the number of its CPUs are exposed via daemon settings: `local.<instance-name>.(cpus|disk|memory)`.

To modify one of this properties, first stop the instance and then issue the [`set`](reference-command-line-interface-set) command. For example:

```{code-block} text
multipass stop handsome-ling
multipass set local.handsome-ling.cpus=4
multipass set local.handsome-ling.disk=60G
multipass set local.handsome-ling.memory=7G
```

```{note}
The disk size can only be increased.
```

```{caution}
When increasing the disk size of an instance, the partition may not expand automatically to use the new available space. This usually happens if the partition was already full when trying to increase the disk size.

In such cases, you need to expand the partition manually. To do so, `shell` into the instance and run the following command:

```{code-block} text
sudo parted /dev/sda resizepart 1 100%
```

The system will guide you through the configuration steps:

<pre>
Warning: Not all of the space available to /dev/sda appears to be used,
you can fix the GPT to use all of the space (an extra 4194304 blocks)
or continue with the current setting.
Fix/Ignore? <b>fix</b>
Partition number? <b>1</b>
Warning: Partition /dev/sda1 is being used.
Are you sure you want to continue? Yes/No? <b>yes</b>
</pre>

When done, run `sudo resize2fs /dev/sda1`.
```

You can view these properties using the `get` command, without the need to stop instances. For example, `multipass get local.handsome-ling.cpus` returns the configured number of CPUs, which in our example is "4".

```{note}
You can only update the properties of `Stopped`, non-deleted instances. If you try to update an instance that is in `Running`, `Suspended`, or `Deleted` status you'll incur an error.

On the other hand, it's always possible to fetch properties for all instances. Use `multipass get --keys` to obtain their settings keys.
```

```{note}
Modifying instance settings is not supported when using the Hyperkit driver, which has been deprecated in favour of QEMU. The QEMU and VirtualBox drivers on Intel-based macOS hosts do support instance modification.
```

## Set the status of an instance to primary

> See also:  [client.primary-name](reference-settings-client-primary-name)

This section demonstrates how to set the status of an instance to primary. This is convenient because it makes this instance the default argument for several commands, such as `shell` , `start` , `stop` , `restart` and `suspend`, and also automatically mounts your $HOME directory into the instance.

To grant a regular instance the primary status, assign its name to the `client.primary-name`:

```{code-block} text
multipass set client.primary-name=<instance name>
```

This setting allows transferring primary status among instances. The primary instance's name can be configured independently of whether instances with the old and new names exist. If they do, they lose and gain primary status accordingly.

This provides a means of (de)selecting an existing instance as primary.

### Example

Assign the primary status to an instance called "first":

```{code-block} text
multipass set client.primary-name=first
```

This instance is picked up automatically by `multipass start`. When you run this command, the primary instance also automatically mounts the user's home directory into a directory called `Home`:

```{code-block} text
...
Launched: first
Mounted '/home/ubuntu' into 'first:Home'
```

Run `multipass stop` to stop the primary instance, and then launch another instance named "second":

```{code-block} text
multipass launch --name second
```

Now, change the primary instance to the existing "second" instance:

```{code-block} text
multipass set client.primary-name=second
```

From now on, the "second" instance will be used as the *primary* instance, so for example it will be used by default when you run the command `multipass suspend`.

When listing instances, the primary one is displayed first. For example, if you run `multipass list` now, you'll see:

```{code-block} text
Name                    State             IPv4             Image
second                  Suspended         --               Ubuntu 18.04 LTS
first                   Stopped           --               Ubuntu 18.04 LTS
```
