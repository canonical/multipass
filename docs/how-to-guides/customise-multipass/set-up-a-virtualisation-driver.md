(how-to-guides-customise-multipass-set-up-a-virtualisation-driver)=
# Set up a virtualisation driver

> See also: {ref}`explanation-driver`, {ref}`reference-settings-local-driver`

This document demonstrates how to change to an alternative virtualisation driver and how to switch back to the default driver.

Be aware that Multipass already has sensible defaults, so this is an optional step.

```{note}
Changing your driver will also change its hypervisor.

````

## Default driver

`````{tabs}

````{group-tab} Linux

By default, Multipass on Linux uses the `qemu` driver.

````

````{group-tab} macOS

By default, Multipass on macOS uses the `qemu` driver.

````

````{group-tab} Windows

By default, Multipass on Windows uses the `hyperv` driver.

````

`````

## Install an alternative driver

`````{tabs}

````{group-tab} macOS

If you want to (or have to), you can switch to VirtualBox instead of using the default hypervisor that Multipass uses.

To switch the Multipass driver to VirtualBox, run this command:

```{code-block} text
sudo multipass set local.driver=virtualbox
```

From now on, all instances started with `multipass launch` will use VirtualBox behind the scenes.

````

````{group-tab} Windows

If you want to (or have to), you can change the hypervisor that Multipass uses to VirtualBox.

To that end, install VirtualBox, if you haven't yet. You may find that you need to <a href="https://forums.virtualbox.org/viewtopic.php?f=6&t=88405#p423658">run the VirtualBox installer as administrator</a>.

<!-- Sphinx doesn't like the & character in the above link, the only way to make it work is using basic HTML syntax. The link was:
[run the VirtualBox installer as administrator](https://forums.virtualbox.org/viewtopic.php?f=6&t=88405#p423658)
-->

To switch the Multipass driver to VirtualBox (also with Administrator privileges), run this command:

```{code-block} powershell
multipass set local.driver=virtualbox
```

From then on, all instances started with `multipass launch` will use VirtualBox behind the scenes.

````

`````

## Switch back to the default driver

> See also: {ref}`reference-command-line-interface-stop`, {ref}`reference-settings-local-driver`

`````{tabs}

````{group-tab} Linux

To switch back to the default `qemu` driver, run the following commands:

```{code-block} text
multipass stop --all
multipass set local.driver=qemu
```

All your existing instances will be migrated and can be used straight away.

````

````{group-tab} macOS

If you want to switch back to the default driver, run the following command:

```{code-block} text
multipass set local.driver=qemu
```

Instances created with VirtualBox don't get transferred, but you can always come back to them.

````

````{group-tab} Windows

If you want to switch back to the default driver, run the following command:

```{code-block} text
multipass set local.driver=hyperv
```

Instances created with VirtualBox don't get transferred, but you can always come back to them.

````

`````
