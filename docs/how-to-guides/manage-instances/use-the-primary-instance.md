(how-to-guides-manage-instances-use-the-primary-instance)=
# Use the primary instance

> See also: [Instance](explanation-instance), [client.primary-name](reference-settings-client-primary-name), [`shell`](reference-command-line-interface-shell), [`mount`](reference-command-line-interface-mount)

Multipass offers a quick way to access an Ubuntu instance via a simple `multipass shell` command. This is achieved via the so-called {ref}`primary-instance` that is also automatically created (if it doesn't exist) when the user runs the [`multipass start`](reference-command-line-interface-start) or [`multipass shell`](reference-command-line-interface-shell) commands without any arguments.

When automatically created, the primary instance gets the same properties as if [`launch`](reference-command-line-interface-launch) was used with no arguments, except for the name (`primary` by default). In particular, this means that the instance will use the latest Ubuntu LTS image and will have the default CPU, disk and memory configuration.

You can also launch the primary instance with additional parameters, as you would do for any other instance. This provides one way to fine-tune its properties (e.g. `multipass launch --name primary --cpus 4 lts`). Alternatively, you can set another instance as primary, as explained in {ref}`changing-the-primary-instance` below.

There can be only one primary instance at any moment. If it exists, it is always listed first in the output of `list` and `info`.

## Steering the primary instance

The primary instance can also be controlled from the {ref}`gui-client-tray-icon`

In the command line, it is used as the default when no instance name is specified in the `shell`, `start`, `stop`, `restart` and `suspend` commands. When issuing one of these commands with no positional arguments, the primary instance is targeted. Its name can still be given explicitly wherever an instance name is expected (e.g. `multipass start primary`).

## Automatic home mount

When launching the primary instance, whether implicitly or explicitly, Multipass automatically mounts the user's home inside it, in the folder `Home`. As with any other mount, you can unmount it with `multipass umount`. For instance, `multipass umount primary` will unmount all mounts made by Multipass inside `primary`, including the auto-mounted `Home`.

```{note}
On Windows mounts are disabled by default for security reasons. See [`set`](reference-command-line-interface-set) and [local.privileged-mounts](reference-settings-local-privileged-mounts) for information on how to enable them if needed.
```

(changing-the-primary-instance)=
## Changing the primary instance

The primary instance is identified as such by its name. The name that designates an instance as the primary one is determined by a configuration setting with the key `client.primary-name`. In other words, while `primary` is the default name of the primary instance, you can change it with `multipass set client.primary-name=<custom_name>`.

This setting allows transferring primary status among instances. You can configure the primary name independently of whether instances with the old and new names exist. If they do, they lose and gain primary status accordingly.

This provides a means of (de)selecting an existing instance as primary. For example, after `multipass set client.primary-name=first`, the primary instance would be called `first`. A subsequent `multipass start` would start `first` if it existed, and launch it otherwise.

## Example

Here is a long-form example of how Multipass handles the primary instance.

Set the name of the primary instance and start it:

```{code-block} text
multipass set client.primary-name=first
multipass start
```

Sample output:

```{code-block} text
Launched: first
Mounted '/home/ubuntu' into 'first:Home'
```

Now, stop the primary and launch another instance:

```{code-block} text
multipass stop
multipass launch eoan
```

Sample output:

```{code-block} text
Launched: calm-chimaera
```

Change the `client.primary-name` setting to the newly launched instance, and review the list of existing instances:

```{code-block} text
multipass set client.primary-name=calm-chimaera
multipass list
```

Sample output:

```{code-block} text
Name                    State             IPv4             Image
calm-chimaera           Running           10.122.139.63    Ubuntu 19.04 LTS
first                   Stopped           --               Ubuntu 18.04 LTS
```
