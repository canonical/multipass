(how-to-guides-manage-instances-share-data-with-an-instance)=
# Share data with an instance

``` {seealso}
[Instance](explanation-instance), [Mount](explanation-mount), [ID mapping](explanation-id-mapping), [`launch`](reference-command-line-interface-launch), [`mount`](reference-command-line-interface-mount), [`umount`](reference-command-line-interface-umount), [`transfer`](reference-command-line-interface-transfer)
```

This guide explains how to share data between your host and an instance. There are two ways to accomplish this:
* the `mount` command, that maps a local folder to a new or existing folder in the instance's filesystem
* the `transfer` command, that copies files to and from an instance

## Using `mount`

You can use the [`mount`](reference-command-line-interface-mount) command to share data between your host and an instance, by making specific folders in your host's filesystem available in your instance's filesystem, with read and write permissions. Mounted paths are persistent, meaning that they will remain available until they are explicitly unmounted.

The basic syntax of the `mount` command is:

```{code-block} text
multipass mount <local path> <instance name>
```

For example, to map your local home directory on a Linux system (identified as $HOME) into the `keen-yak` instance, run this command:

```{code-block} text
multipass mount $HOME keen-yak
```

You can check the result running `multipass info keen-yak`:

```{code-block} text
...
Mounts:         /home/michal => /home/michal
```

From this point the local home directory `/home/michal` will be available inside the instance.

If you want to mount a local directory to a different path in your instance, you can specify the target path as follows:

```{code-block} text
multipass mount $HOME keen-yak:/some/path
```

```{caution}
If the `/some/path` directory already exists in the instance's filesystem, its contents will be temporarily hidden ("overlaid") by the mounted directory, but not overwritten. The original folder remains intact and will be revealed if you unmount.

For this reason, it is not possible to mount an external folder path over the instance's $HOME directory, because it also contains the SSH keys required to access the instance: by hiding them, you would no longer be able to shell into the instance.
```

You can also define mounts when you create an instance, using the [`launch`](reference-command-line-interface-launch) command with the `--mount` option:

```{code-block} text
multipass launch --mount /local/path:/instance/path
```

### Unmounting shared directories

To unmount previously mounted paths, use the [`umount`](reference-command-line-interface-umount) command.

You can specify the folder path to unmount:

```{code-block} text
multipass umount keen-yak:/home/michal
```

or, if you don't specify any paths, unmount all shared folders at once:

```{code-block} text
multipass umount keen-yak
```

## Using `transfer`

You can also use the [`transfer`](reference-command-line-interface-transfer) command to copy files from your local filesystem to the instance's filesystem, and vice versa.

To indicate that a file is inside an instance, prefix its path with `<instance name>:`.

For example, to copy the `crontab` and `fstab` files from the `/etc` directory on the `keen-yak` instance to the `/home/michal` folder in the host's filesystem:

```{code-block} text
multipass transfer keen-yak:/etc/crontab keen-yak:/etc/fstab /home/michal
```

The files will be copied with the correct user mapping, as you'll see running the `ls -l /home/michal` command:

```{code-block} text
...
-rw-r--r-- 1 michal michal 722 Oct 18 12:13 /home/michal/crontab
-rw-r--r-- 1 michal michal  82 Oct 18 12:13 /home/michal/fstab
...
```

The other way around, if you want to copy these files from your local filesystem into the instance, run the command:

```{code-block} text
multipass transfer /etc/crontab /etc/fstab keen-yak:/home/michal
```

In this case, the output of the `ls -l /home/michal` command on the instance will be:
```{code-block} text
...
-rw-rw-r-- 1 ubuntu ubuntu 722 Oct 18 12:14 crontab
-rw-rw-r-- 1 ubuntu ubuntu  82 Oct 18 12:14 fstab
...
```

See also [ID mapping](explanation-id-mapping) for more information on how the mount command maps user and group IDs between the host and the instance.

<!-- Discourse contributors
<small>**Contributors:** @saviq, @nhart, @andreitoterman, @ricab, @gzanchi </small>
-->
