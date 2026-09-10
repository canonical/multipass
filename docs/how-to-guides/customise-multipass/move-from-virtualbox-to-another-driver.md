(how-to-guides-customise-multipass-move-from-virtualbox-to-another-driver)=

# Move from VirtualBox to another driver

> See also: [`set`](reference-command-line-interface-set),
> [local.driver](reference-settings-local-driver),
> [Driver](explanation-driver),
> [How to set up the driver](how-to-guides-customise-multipass-set-up-the-driver)

As of Multipass 1.17, the VirtualBox driver is being deprecated and will be removed in an upcoming
release. Unlike other driver deprecations, no migration of VirtualBox instances is planned.
Multipass will warn VirtualBox users of the deprecation and ask them to move away as soon as
possible.

We recommend switching to the platform-appropriate replacement driver:

`````{tab-set}

````{tab-item} macOS
:sync: macOS

Switch to `applevz`:

```{code-block} text
multipass set local.driver=applevz
```

````

````{tab-item} Windows
:sync: Windows

Switch to `hyperv_api`:

```{code-block} text
multipass set local.driver=hyperv_api
```

````

`````

Your VirtualBox instances are not destroyed when you switch, but they become unreachable from the
new driver — Multipass keeps separate instance scopes per driver (see [Driver](explanation-driver)).
You can switch back to `virtualbox` for now if you still need those instances, but once the driver
is removed, you will need to manually recreate any instances you want to keep under the new
driver.

## Recreating instances under the new driver

Since there is no migration path, moving an instance means launching a fresh one under the new
driver and bringing over anything you need from the old one, for example:

```{code-block} text
multipass set local.driver=virtualbox
multipass mount <local-path> <instance>:<path>   # or: multipass transfer <instance>:<path> <local-path>, to copy files out
multipass set local.driver=<new-driver>
multipass launch --name <instance> ...
multipass mount <local-path> <instance>:<path>   # or: multipass transfer <local-path> <instance>:<path>, to copy files in
```

Once you have recreated the instances you need, you can delete the old VirtualBox ones by
temporarily switching back:

```{code-block} text
multipass set local.driver=virtualbox
multipass delete [-p] <instance> [...]
```

You can move back and forth between `virtualbox` and the new driver as many times as you want until
VirtualBox is removed. Apart from the deprecation warning, old functionality remains the same until
then.
