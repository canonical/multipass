(how-to-guides-manage-instances-use-an-instance)=
# Use an instance

> See also: [Instance](explanation-instance)

This document demonstrates various ways to use an instance.

## Open a shell prompt inside an instance

> See also: [`shell`](reference-command-line-interface-shell)

To open a shell prompt on an existing instance (e.g. `loving-duck`), run the command `multipass shell loving-duck`. The output will be similar to the following:

```{code-block} text
Welcome to Ubuntu 20.04.4 LTS (GNU/Linux 5.4.0-109-generic x86_64)

 * Documentation:  https://help.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/advantage

  System information as of Tue May 31 14:26:40 -03 2022

  System load:  0.0               Processes:             113
  Usage of /:   28.8% of 4.67GB   Users logged in:       0
  Memory usage: 21%               IPv4 address for ens3: 10.49.93.241
  Swap usage:   0%


1 update can be applied immediately.
To see these additional updates run: apt list --upgradable


The list of available updates is more than a week old.
To check for new updates run: sudo apt update

To run a command as administrator (user "root"), use "sudo <command>".
See "man sudo_root" for details.

ubuntu@loving-duck:~$
```

If the instance `loving-duck` is `Stopped` or `Suspended`, it will be started automatically.

If no argument is given to the `shell` command, Multipass will open a shell prompt on the primary instance (and also create it, if it doesn't exist).

As shown in the example above, an Ubuntu prompt is displayed as a result of the `shell` command, where you can run commands inside the instance.

To end the session, use `logout`, `exit`, or the `Ctrl-D` shortcut.

```{note}
This is also available on the GUI.
```

## Run a command inside an instance

> See also: [`exec`](reference-command-line-interface-exec)

To run a single command inside an instance, you don't need to open a shell. The command can be directly called from the host using `multipass exec`. For example, the command `multipass exec loving-duck -- pwd` returns:

```{code-block} text
/home/ubuntu
```

In the example, `/home/ubuntu` is the output of invoking the `pwd` command on the `loving-duck` instance.

## Start an instance

> See also: [`start`](reference-command-line-interface-start)

An existing instance that is in `Stopped` or `Suspended` status can be started with the `multipass start` command; for example:

```{code-block} text
multipass start loving-duck
```

You can start multiple instances at once, specifying the instance names in the command line:

```{code-block} text
multipass start loving-duck devoted-lionfish sensible-shark
```

To start all existing instances at once, use the `--all` option:

```{code-block} text
multipass start --all
```

If no options are specified, the `multipass start` command starts the primary instance, creating it if needed.

```{note}
This is also available on the GUI.
```

## Suspend an instance

> See also: [`suspend`](reference-command-line-interface-suspend)

An instance can be suspended with the command:

```{code-block} text
multipass suspend loving-duck
```

You can suspend multiple instances at once, specifying the instance names in the command line:

```{code-block} text
multipass suspend loving-duck devoted-lionfish sensible-shark
```

To suspend all running instances at once, use the `--all` option:

```{code-block} text
multipass suspend --all
```

If no options are specified, the `multipass suspend` command suspends the primary instance, if it exists and is running.

## Stop an instance

> See also: [`stop`](reference-command-line-interface-stop)

A running, not suspended instance is stopped with the command:

```{code-block} text
multipass stop loving-duck
```

You can stop multiple instances at once, specifying the instance names in the command line:

```{code-block} text
multipass stop loving-duck devoted-lionfish sensible-shark
```

To stop all running instances at once, use the `--all` option:

```{code-block} text
multipass stop --all
```

If no options are specified, the `multipass stop` command stops the primary instance, if it exists and is running.

### Stop an instance forcefully

If the `multipass stop` command doesn’t work, you can use the `--force` argument to force the instance to shut down immediately. This is particularly useful when the virtual machine is in a non-responsive, unknown or suspended state.

```{code-block} text
multipass stop --force
```

```{caution}
The `stop --force` command is analogous to unplugging the power cord from a physical machine – it immediately halts all computing activities. This may be necessary under certain circumstances but can potentially lead to data loss or corruption.
```

```{note}
This command is also available on the Multipass GUI.
```

<!--?TODO: ADD RESTART?-->
