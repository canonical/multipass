(how-to-guides-customise-multipass-view-multipass-instances-using-the-driver)=
# View Multipass instances using the driver

> See also: {ref}`explanation-driver`, {ref}`reference-settings-local-driver`

This document demonstrates how to view Multipass instances using the driver.

Prerequisite: Have a running Multipass instance.

`````{tabs}

````{group-tab} Linux

You can view instances with libvirt in two ways, using the `virsh` CLI or the [`virt-manager` GUI](https://virt-manager.org/).

To use the `virsh` CLI, launch an instance and then run the command `virsh list` (see [`man virsh`](https://manpages.ubuntu.com/manpages/questing/en/man1/virsh.1.html) for a command reference):

```{code-block} text
virsh list
```

The output will be similar to the following:

```{code-block} text
 Id   Name                   State
--------------------------------------
 1    unaffected-gyrfalcon   running
```
Alternatively, to use the `virt-manager` GUI, ...

```{figure} /images/multipass-virt-manager-gui.png
   :width: 584px
   :alt: Virtual Machine Manager GUI
```

<!-- Original image on the Asset Manager
![Virtual Machine Manager GUI|584x344](https://assets.ubuntu.com/v1/51cf2c57-multipass-virt-manager-gui.png)
-->

````

````{group-tab} macOS

Multipass runs as the `root` user, so to see the instances in  VirtualBox, or through the `VBoxManage` command, you have to run those as `root`, too.

To see the instances in VirtualBox, use the command:

```{code-block} text
sudo VirtualBox
```

```{figure} /images/multipass-macos-virtualbox-manager.png
   :width: 720px
   :alt: Multipass instances in VirtualBox
```

<!-- Original image on the Asset Manager
![Multipass instances in VirtualBox](https://assets.ubuntu.com/v1/9c959eed-multipass-macos-virtualbox-manager.png)
-->

And, to list the instances on the command line, run:

```{code-block} text
sudo VBoxManage list vms
```

Sample output:
```{code-block} text
"primary" {395d5300-557d-4640-a43a-48100b10e098}
```

```{note}
You can still use the `multipass` client and the system menu icon, and any changes you make to the configuration of the instances in VirtualBox will be persistent. They may not be represented in Multipass commands such as `multipass info` , though.
[/note]

````

````{group-tab} Windows

Multipass runs as the _System_ account. To see the instances in VirtualBox, or through the `VBoxManage` command, you have to run those as that user via [`PsExec -s`](https://docs.microsoft.com/en-us/sysinternals/downloads/psexec).

Download and unpack [PSTools.zip](https://download.sysinternals.com/files/PSTools.zip) in your *Downloads* folder, and in an administrative PowerShell, run:

```{code-block} powershell
& $env:USERPROFILE\Downloads\PSTools\PsExec.exe -s -i $env:VBOX_MSI_INSTALL_PATH\VirtualBox.exe
```

```{figure} /images/multipass-windows-virtualbox-manager.png
   :width: 720px
   :alt: Multipass instances in VirtualBox
```

<!-- Original image on the Asset Manager
![Multipass instances in VirtualBox](https://assets.ubuntu.com/v1/edce2443-multipass-windows-virtualbox-manager.png)
-->

To list the instances on the command line:

```{code-block} powershell
& $env:USERPROFILE\Downloads\PSTools\PsExec.exe -s $env:VBOX_MSI_INSTALL_PATH\VBoxManage.exe list vms
```

Sample output:

```{code-block} powershell
"primary" {05a04fa0-8caf-4c35-9d21-ceddfe031e6f}
```

```{note}
You can still use the `multipass` client and the system menu icon, and any changes you make to the configuration of the instances in VirtualBox will be persistent. They may not be represented in Multipass commands such as `multipass info`, though.

````

`````
