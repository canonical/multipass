(how-to-guides-customise-multipass-configure-where-multipass-stores-external-data)=
# Configure where Multipass stores external data

This document demonstrates how to configure the location where Multipass stores instances, caches images, and other data. Configuring a new storage location can be useful, for example, if you need to free up storage space on your boot partition.

(configuring-a-new-storage-location)=
## Configuring a new storage location

```{caution}
**Caveats:**
- Multipass will not migrate your existing data; this article explains how to do it manually. If you do not transfer the data, you will have to re-download any Ubuntu images and reinitialize any instances that you need.
- When uninstalling Multipass, the uninstaller will not remove data stored in custom locations, so you'll have to deleted it manually.
```

`````{tab-set} 

````{tab-item} Linux

First, stop the Multipass daemon:

```{code-block} text
sudo snap stop multipass
```

Since Multipass is installed using a strictly confined snap, it is limited on what it can do or access on your host. Depending on where the new storage directory is located, you will need to connect the respective interface to the Multipass snap. Because of [snap confinement](https://snapcraft.io/docs/snap-confinement), this directory needs to be located in either `/home` (connected by default) or one of the removable mounts points (`/mnt` or `/media`). To connect the removable mount points, use the command: 

  ```{code-block} text
  sudo snap connect multipass:removable-media
  ```

Create the new directory in which Multipass will store its data:

```{code-block} text
mkdir -p <path>
sudo chown root <path>
```

After that, create the override config file, replacing `<path>` with the absolute path of the directory created above.

```{code-block} text
sudo mkdir /etc/systemd/system/snap.multipass.multipassd.service.d/
sudo tee /etc/systemd/system/snap.multipass.multipassd.service.d/override.conf <<EOF
[Service]
Environment=MULTIPASS_STORAGE=<path>
EOF
```

The output at this point will be:
```{code-block} text
[Service]
Environment=MULTIPASS_STORAGE=<path>
```

Then, instruct `systemd` to reload the daemon configuration files:

```{code-block} text
sudo systemctl daemon-reload
```

Now you can transfer the data from its original location to the new location:

```{code-block} text
sudo cp -r /var/snap/multipass/common/data/multipassd <path>/data
sudo cp -r /var/snap/multipass/common/cache/multipassd <path>/cache
```

<!-- The following step was added to address GitHub issue https://github.com/canonical/multipass/issues/3254 -->

You also need to edit the following configuration files so that the specified paths point to the new Multipass storage directory, otherwise your instances will fail to start:

* `multipass-vm-instances.json`: Update the absolute path of the instance images in the "arguments" key for each instance.
* `vault/multipassd-instance-image-records.json`: Update the "path" key for each instance.

Finally, start the Multipass daemon:

```{code-block} text
sudo snap start multipass
```

You can delete the original data at your discretion, to free up space:

```{code-block} text
sudo rm -rf /var/snap/multipass/common/data/multipassd
sudo rm -rf /var/snap/multipass/common/cache/multipassd
```

````

````{tab-item} macOS

First, become `root`:

```{code-block} text
sudo su
```

Stop the Multipass daemon:

```{code-block} text
launchctl unload /Library/LaunchDaemons/com.canonical.multipassd.plist
```

Move your current data from its original location to `<path>`, replacing `<path>` with your custom location of choice:

```{code-block} text
mv /var/root/Library/Application\ Support/multipassd <path>
```

```{caution}
Make sure the `multipassd` directory is moved to `<path>`, and not inside the  `<path>` folder.
```

Define a symbolic link from the original location to the absolute path of new location:

```{code-block} text
ln -s <path> /var/root/Library/Application\ Support/multipassd
```

Finally, start the Multipass daemon:

```{code-block} text
launchctl load /Library/LaunchDaemons/com.canonical.multipassd.plist
```

````

````{tab-item} Windows

First, open a PowerShell prompt with administration privileges.

Stop the Multipass daemon:

```{code-block} powershell
Stop-Service Multipass
```

Create and set the new storage location, replacing `<path>` with the absolute path of your choice:

```{code-block} powershell
New-Item -ItemType Directory -Path "<path>"
Set-ItemProperty -Path "HKLM:System\CurrentControlSet\Control\Session Manager\Environment" -Name MULTIPASS_STORAGE -Value "<path>"
```

Now you can transfer the data from its original location to the new location:

```{code-block} powershell
Copy-Item -Path "C:\ProgramData\Multipass\*" -Destination "<path>" -Recurse
```

```{caution}
It is important to copy any existing data to the new location. This avoids unauthenticated client issues, permission issues, and in general, to have any previously created instances available.
```

Finally, start the Multipass daemon:

```{code-block} powershell
Start-Service Multipass
```

You can delete the original data at your discretion, to free up space:

```{code-block} powershell
Remove-Item -Path "C:\ProgramData\Multipass\*" -Recurse
```

````

`````

## Reverting back to the default location

`````{tab-set}

````{tab-item} Linux

Stop the Multipass daemon:

```{code-block} text
sudo snap stop multipass
```

Although not required, to make sure that Multipass does not have access to directories that it shouldn't, you can disconnect the respective interface depending on where the custom storage location was set (see {ref}`configuring-a-new-storage-location` above).
For example, to disconnect the removable mounts points (`/mnt` or `/media`), run:

```{code-block} text
sudo snap disconnect multipass:removable-media
```

Then, remove the override config file:

```{code-block} text
sudo rm /etc/systemd/system/snap.multipass.multipassd.service.d/override.conf
sudo systemctl daemon-reload
```

Now you can transfer your data from the custom location back to its original location:

```{code-block} text
sudo cp -r <path>/data /var/snap/multipass/common/data/multipassd
sudo cp -r <path>/cache /var/snap/multipass/common/cache/multipassd
```

Finally, start the Multipass daemon:

```{code-block} text
sudo snap start multipass
```

You can delete the data from the custom location at your discretion, to free up space:

```{code-block} text
sudo rm -rf <path>
```

````

````{tab-item} macOS

First, become `root`:

```{code-block} text
sudo su
```

Stop the Multipass daemon:

```{code-block} text
launchctl unload /Library/LaunchDaemons/com.canonical.multipassd.plist
```

Remove the link pointing to your custom location:

```{code-block} text
unlink /var/root/Library/Application\ Support/multipassd
```

Move the data from your custom location back to its original location:

```{code-block} text
mv <path> /var/root/Library/Application\ Support/multipassd
```

Finally, start the Multipass daemon:

```{code-block} text
launchctl load /Library/LaunchDaemons/com.canonical.multipassd.plist
```

````

````{tab-item} Windows

First, open a PowerShell prompt with administrator privileges.

Stop the Multipass daemon:

```{code-block} powershell
Stop-Service Multipass
```

Remove the setting for the custom storage location:

```{code-block} powershell
Remove-ItemProperty -Path "HKLM:System\CurrentControlSet\Control\Session Manager\Environment" -Name MULTIPASS_STORAGE
```

Now you can transfer the data back to its original location:

```{code-block} powershell
Copy-Item -Path "<path>\*" -Destination "C:\ProgramData\Multipass" -Recurse
```

Finally, start the Multipass daemon:

```{code-block} powershell
Start-Service Multipass
```

You can delete the data from the custom location at your discretion, to free up space:

```{code-block} powershell
Remove-Item -Path "<path>" -Recurse
```

````

`````

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/configure-multipass-storage" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
