(reference-command-line-interface-mount)=
# mount

> See also: [How to share data with an instance](/how-to-guides/manage-instances/share-data-with-an-instance), [Mount](/explanation/mount), [`umount`](/reference/command-line-interface/umount)

The `multipass mount` command maps a local directory from the host to an instance, with the possibility to specify the mount type (classic or native) and define group or user [ID mappings](/explanation/id-mapping).

For example, here's the syntax for mapping a local directory to your virtual machine using the classic mount:

```{code-block} text
multipass mount --type=classic /host/path <instance name>:/instance/path
```

Use the `multipass umount` command to undo the mapping.

See [Mount](/explanation/mount) to learn more on the difference between "classic" and "native" mounts.

See [How to share data with an instance](/how-to-guides/manage-instances/share-data-with-an-instance) for examples of how you can use the `multipass mount` command to share data between your host and an instance.

---

The full `multipass help mount` output explains the available options:

```{code-block} text
Usage: multipass mount [options] <source> <target> [<target> ...]
Mount a local directory inside the instance. If the instance is
not currently running, the directory will be mounted
automatically on next boot.

Options:
  -h, --help                       Displays help on commandline options
  -v, --verbose                    Increase logging verbosity. Repeat the 'v'
                                   in the short option for more detail. Maximum
                                   verbosity is obtained with 4 (or more) v's,
                                   i.e. -vvvv.
  -g, --gid-map <host>:<instance>  A mapping of group IDs for use in the mount.
                                   File and folder ownership will be mapped from
                                   <host> to <instance> inside the instance. Can
                                   be used multiple times. Mappings can only be
                                   specified as a one-to-one relationship.
  -u, --uid-map <host>:<instance>  A mapping of user IDs for use in the mount.
                                   File and folder ownership will be mapped from
                                   <host> to <instance> inside the instance. Can
                                   be used multiple times. Mappings can only be
                                   specified as a one-to-one relationship.
  -t, --type <type>                Specify the type of mount to use.
                                   Classic mounts use technology built into
                                   Multipass.
                                   Native mounts use hypervisor and/or platform
                                   specific mounts.
                                   Valid types are: 'classic' (default) and
                                   'native'

Arguments:
  source                           Path of the local directory to mount
  target                           Target mount points, in <name>[:<path>]
                                   format, where <name> is an instance name,
                                   and optional <path> is the mount point.
                                   If omitted, the mount point will be under
                                   /home/ubuntu/<source-dir>, where <source-dir>
                                   is the name of the <source> directory.
```

<!-- Discourse contributors
<small>**Contributors:** @georgeliaojia, @sharder996, @davidekete, @gzanchi </small>
-->
