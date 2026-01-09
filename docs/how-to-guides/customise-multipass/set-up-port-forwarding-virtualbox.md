(how-to-guides-customise-multipass-set-up-port-forwarding-for-a-multipass-instance-virtualbox)=
# Set up port forwarding for a Multipass instance using Virtualbox

```{seealso}
{ref}`explanation-driver`, {ref}`reference-settings-local-driver`
```

This document demonstrates how to set up port forwarding for a Multipass instance using VirtualBox.

`````{tabs}

````{group-tab} Linux

This option only applies to macOS and Windows systems.

````

````{group-tab} macOS

To expose a service running inside the instance on your host, you can use [VirtualBox's port forwarding feature](https://www.virtualbox.org/manual/ch06.html#natforward).

For example:

```{code-block} text
sudo VBoxManage controlvm "primary" natpf1 "myservice,tcp,,8080,,8081"
```

You can then open, say, https://localhost:8081/, and the service running inside the instance on port 8080 will be exposed.

````

````{group-tab} Windows

To expose a service running inside the instance on your host, you can use [VirtualBox's port forwarding feature](https://www.virtualbox.org/manual/ch06.html#natforward).

For example:

```{code-block} powershell
& $env:USERPROFILE\Downloads\PSTools\PsExec.exe -s $env:VBOX_MSI_INSTALL_PATH\VBoxManage.exe controlvm "primary" natpf1 "myservice,tcp,,8080,,8081"
```

You can then open, say, https://localhost:8081/, and the service running inside the instance on port 8080 will be exposed.

````

`````
