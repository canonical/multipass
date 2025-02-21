(explanation-instance)=
# Instance

> See also: [How to manage instances](/how-to-guides/manage-instances/index), [Instance states](/reference/instance-states), [Mount](/explanation/mount)

An **instance** is a virtual machine created and managed by Multipass.

> For more information on the naming convention, see [Instance name format](/reference/instance-name-format).

(primary-instance)=
## Primary instance

The Multipass [Command-Line Interface](/reference/command-line-interface/index) (CLI) provides a few shortcuts using a special instance, called *primary* instance. By default, this is the instance named `primary`.

When invoked without positional arguments, state transition commands — [`start`](/reference/command-line-interface/start), [`restart`](/reference/command-line-interface/restart), [`stop`](/reference/command-line-interface/stop), and [`suspend`](/reference/command-line-interface/suspend) — operate on this special instance. So does the [`shell`](/reference/command-line-interface/shell) command. Furthermore, `start` and `shell` create the primary instance if it does not yet exist.

When creating the primary instance, the Multipass CLI client automatically mounts the user's home directory into it. As with any other mount, it can be unmounted with `multipass umount`. For instance, the command `multipass umount primary` will unmount all mounts made by Multipass inside the `primary` instance, including the auto-mounted `Home`.

```{note}
On Windows, mounts are disabled by default for security reasons. For more details, see {ref}`security-considerations-mount`.
```

In all other respects, the primary instance is the same as any other instance. Its properties are the same as if it had been launched manually with `multipass launch --name primary`.

### Selecting the primary instance

The name of the instance that the Multipass CLI treats as primary can be modified with the setting [`client.primary-name`](/reference/settings/client-primary-name). This setting determines the name of the instance that Multipass creates and operates as primary, providing a mechanism to turn any existing instance into the primary instance, as well as disabling the primary feature.
