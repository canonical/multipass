(reference-command-line-interface-launch)=
# launch

The `multipass launch` command without any argument will create and start a new instance based on the default image, using a random generated name; for example:

```
...
Launched: relishing-lionfish
```

You can then shell into an instance by its name:

```{code-block} text
multipass shell relishing-lionfish
```

The only, optional, positional argument is the image to launch an instance from. See [`find`](/reference/command-line-interface/find) for how to find information on the available images.

It's also possible to provide a full URL to the image (use `file://` for an image available on the host running `multipassd`).

You can change the resources made available to the instance by passing the `--cpus`, `--disk` or `--memory` options, changing the allocated CPU cores, disk space or RAM, respectively.

If you want your instance to have a name of your choice, use the `--name` option. The instance name has to be unique and conform to a specific [instance name format](/reference/instance-name-format).

By passing a filename or an URL to `--cloud-init`, you can provide user data to [`cloud-init`](https://cloud-init.io/) to customise the instance on first boot. See [cloud-init documentation](https://cloudinit.readthedocs.io/en/latest/topics/examples.html) for examples.

Use the `--network` option to {ref}`create-an-instance-with-multiple-network-interfaces`.

Passing `--bridged` and `--network bridged` are shortcuts to `--network <name>`, where `<name>` is configured via `multipass set local.bridged-interface`.

You can also mount folders in the instance after it is launched using the  `--mount` option. It can be specified multiple times, with different mount paths.

Use the `--timeout` option to change how long Multipass waits for the machine to boot and initialise.

---

The full `multipass help launch` output explains the available options:

```{code-block} text
Usage: multipass launch [options] [[<remote:>]<image> | <url>]
Create and start a new instance.

Options:
  -h, --help                            Displays help on commandline options
  -v, --verbose                         Increase logging verbosity. Repeat the
                                        'v' in the short option for more detail.
                                        Maximum verbosity is obtained with 4 (or
                                        more) v's, i.e. -vvvv.
  -c, --cpus <cpus>                     Number of CPUs to allocate.
                                        Minimum: 1, default: 1.
  -d, --disk <disk>                     Disk space to allocate. Positive
                                        integers, in bytes, or decimals, with K,
                                        M, G suffix.
                                        Minimum: 512M, default: 5G.
  -m, --memory <memory>                 Amount of memory to allocate. Positive
                                        integers, in bytes, or decimals, with K,
                                        M, G suffix.
                                        Minimum: 128M, default: 1G.
  -n, --name <name>                     Name for the instance. If it is
                                        'primary' (the configured primary
                                        instance name), the user's home
                                        directory is mounted inside the newly
                                        launched instance, in 'Home'.
                                        Valid names must consist of letters,
                                        numbers, or hyphens, must start with a
                                        letter, and must end with an
                                        alphanumeric character.
  --cloud-init <file> | <url>           Path or URL to a user-data cloud-init
                                        configuration, or '-' for stdin
  --network <spec>                      Add a network interface to the
                                        instance, where <spec> is in the
                                        "key=value,key=value" format, with the
                                        following keys available:
                                         name: the network to connect to
                                        (required), use the networks command for
                                        a list of possible values, or use
                                        'bridged' to use the interface
                                        configured via `multipass set
                                        local.bridged-network`.
                                         mode: auto|manual (default: auto)
                                         mac: hardware address (default:
                                        random).
                                        You can also use a shortcut of "<name>"
                                        to mean "name=<name>".
  --bridged                             Adds one `--network bridged` network.
  --mount <local-path>:<instance-path>  Mount a local directory inside the
                                        instance. If <target> is omitted,
                                        the mount point will be under
                                        /home/ubuntu/<source-dir>, where
                                        <source-dir> is the name of the
                                        <source> directory.
  --timeout <timeout>                   Maximum time, in seconds, to wait for
                                        the command to complete. Note that some
                                        background operations may continue
                                        beyond that. By default, instance
                                        startup and initialisation is limited to
                                        5 minutes each.

Arguments:
  image                                 Optional image to launch. If omitted,
                                        then the default Ubuntu LTS will be
                                        used.
                                        <remote> can be either ‘release’ or
                                        ‘daily‘. If <remote> is omitted,
                                        ‘release’ will be used.
                                        <image> can be a partial image hash or
                                        an Ubuntu release version, codename or
                                        alias.
                                        <url> is a custom image URL that is in
                                        https://, https://, or file:// format.
```
