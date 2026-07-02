(how-to-guides-troubleshoot-troubleshoot-launch-start-issues)=
# Troubleshoot launch/start issues

<!-- this topic originates from Ricardo's suggestions here: https://github.com/canonical/multipass/discussions/3660#discussioncomment-10548808 -->

This topic addresses common issues when launching or starting instances, such as *timeouts or "unknown state" errors*.

These problems can occur for a few different reasons. Since Multipass relies on instances having an IP address on the default interface to establish an SSH connection, they are often (but not always) linked to IP assignment or connectivity issues.

The possible reasons that can lead the `launch` or `start` commands to fail are:

1. When you launch a new instance, it fails to load the required image due to stale network cache.
2. The VM didn't manage to boot properly and didn't get to the point where it requests an IP address.
3. The VM requested an IP address, but didn't obtain one.
4. The VM obtained an IP address, but:
    1. Multipass can't find it.
    2. Multipass finds an IP that doesn't match the one that was assigned to the instance.
5. SSH doesn't function properly in the VM, or Multipass is blocked from accessing it.
6. When you launch a new instance, it times out waiting for initialisation to complete.

## Diagnose your issue

Follow these steps to diagnose your issue and identify the most likely scenario:

1. If the `multipass launch` command fails with the message "Downloaded image hash does not match", see {ref}`launch-start-issues-stale-network-cache`.

2. *(Windows, Hyper-V driver)* Inspect the file `C:\WINDOWS\System32\drivers\etc\hosts.ics` and see if there is more than one entry with your instance name in it. If that's the case, see {ref}`launch-start-issues-stale-sharing-lease`.

3. *(Linux/macOS, QEMU driver)* Inspect the Multipass logs and look for a message mentioning `NIC_RX_FILTER_CHANGED`. This message indicates that the network interface has been initialised.
    * If you don't find it, it means that the VM didn't manage to bring up the interface; see {ref}`launch-start-issues-vm-boot-failure`.
    * If the message is present, proceed to check DHCP traffic in the next step.

4. *(Linux/macOS, QEMU driver)* Check DHCP traffic from your host to the instance, to find out if there are requests and replies. Adapt and run the following command *right after starting/launching* the instance:

    ```{code-block} text
    sudo tcpdump -i <bridge> udp port 67 and port 68
    ```

    You will need to replace `<bridge>` with `mpqemubr0` on Linux and with `bridge100` on macOS.

    ```{note}
    Note that, on macOS, `bridge100` is a virtual network interface that only appears when at least a VM is running.
    ```

    * If you see `NIC_RX_FILTER_CHANGED`, you should also see DHCP requests. If you don't, see {ref}`launch-start-issues-vm-boot-failure` and please [let us know](https://github.com/canonical/multipass/issues/new/choose).
    * If you see a DHCP request, but no reply, it means that the VM is still waiting for an IP address to be assigned; see {ref}`launch-start-issues-no-ip-assigned`.
    * If you see DHCP requests and replies, continue to the next step.

5. Look for messages regarding SSH in Multipass's logs. The instance may have obtained an IP and/or be properly connected, but still refuse Multipass when it tries to SSH into it.

6. Look for the message in the CLI or GUI spinner. Once it reads "Waiting for initialisation to complete", Multipass will have succeeded SSH-ing into the instance but remain waiting for cloud-init to finish.

## Troubleshooting steps

(launch-start-issues-vm-boot-failure)=
### VM boot failure

To find out if something is failing during boot, you'd need to attach to the VM's console/serial and observe the output and try to find out where the VM is getting stuck. Here is how you can do that, depending on the driver:

- *(Linux/macOS, QEMU driver)* Relaunch QEMU manually:
    1. Look for the `qemu-system-*` command line corresponding to the failing VM in Multipass logs.
    2. Copy it to an editor and modify it:
        1. Remove `-serial chardev:char0 -nographic`.
        2. Escape any spaces in paths (e.g. `Application Support` should become `Application\ Support`).
    3. Run the edited line in a terminal, with `sudo`. Here is an example:
    ```{code-block} text
    /Library/Application\ Support/com.canonical.multipass/bin/qemu-system-aarch64 -machine virt,gic-version=3 -accel hvf -drive file=/Library/Application\ Support/com.canonical.multipass/bin/../Resources/qemu/edk2-aarch64-code.fd,if=pflash,format=raw,readonly=on -cpu host -nic vmnet-shared,model=virtio-net-pci,mac=52:54:00:e2:30:dd -device virtio-scsi-pci,id=scsi0 -drive file=/var/root/Library/Application\ Support/multipassd/qemu/vault/instances/superior-chihuahua/ubuntu-22.04-server-cloudimg-arm64.img,if=none,format=qcow2,discard=unmap,id=hda -device scsi-hd,drive=hda,bus=scsi0.0 -smp 2 -m 4096M -qmp stdio -cdrom /var/root/Library/Application\ Support/multipassd/qemu/vault/instances/superior-chihuahua/cloud-init-config.iso
    ```
    This will open a QEMU window where you can see the boot output. You may need to select the correct display output (Serial or VGA) from the QEMU menu.

- *(macOS/Windows, VirtualBox driver)* Observe the output in the VirtualBox GUI:
    1. Run the VirtualBox GUI as admin/root:
        - \[macOS] `sudo VirtualBox`
        - \[Windows] Run with `psexec.exe` as explained in {ref}`launch-start-issues-windows-run-virtualbox`.
    2. Start or launch the instance with `multipass start|launch`.
    3. Select and attach to the VM in the VirtualBox GUI and observe the boot output. If it eventually arrives at a login screen, it means that the instance should've started correctly.

- *(Windows, Hyper-V driver)*
    1. Open the Hyper-V Manager GUI (look for it in your Start menu).
    2. Start or launch the instance with `multipass start|launch`.
    2. Select the VM in Hyper-V manager and click "Connect" on the Actions pane, at the right-hand side. Observe the boot output.

#### VM image corruption

Boot failures are often caused by VM image corruption, which can happen when the VM is killed without a proper shutdown.

Here are some options to attempt recovery:

- If you took a [snapshot](/explanation/snapshot) before incurring this issue, you could try to restore it. However, snapshots are typically stored layers against an original image file, so they may not be enough.
- **Run [`fsck`](https://manpages.ubuntu.com/manpages/questing/en/man8/fsck.8.html) in the Serial Console:**

  The `fsck` tool (short for "file system consistency check") is used to scan the file system for errors and attempt repairs.

  **To use it, access the VM’s console as described above and follow these steps:**

  1. **Access the VM's Console**

     - Use the method appropriate for your driver to access the VM's console, as described in the {ref}`launch-start-issues-vm-boot-failure` section.

  2. **Interrupt the Boot Process**

     - As the VM starts booting, interrupt the boot process to access the GRUB menu:
       - Press the `Esc` key repeatedly during the VM's startup until the GRUB menu appears.
       - On some systems, you might need to hold down the `Shift` key instead.
       - The key needs to be pressed at just the right time, after UEFI loading (to avoid getting into the UEFI screen), but before Ubuntu starts booting (to trigger GRUB)

  3. **Enter Recovery Mode**

     - In the GRUB menu:
       - Use the arrow keys to select **Advanced options for Ubuntu** (or your distribution's equivalent) and press `Enter`.
       - Select a kernel version with `(recovery mode)` appended and press `Enter`.

  4. **Run `fsck` from Recovery Menu**

     - Once in the recovery menu:
       - Use the arrow keys to highlight **`fsck`** and press `Enter`.
       - You will be prompted to remount the filesystem in read/write mode. Select **Yes**.
       - The system will run `fsck` and attempt to repair any detected issues.

  5. **Alternatively, Drop to a Root Shell**

     - If you prefer to run `fsck` manually:
       - From the recovery menu, select **`root`** to drop to a root shell prompt.
       - At the prompt, run the following commands:

         ```{code-block} text
         mount -o remount,ro /
         fsck -f /
         ```

       - After `fsck` completes, remount the filesystem in read/write mode:

         ```{code-block} text
         mount -o remount,rw /
         ```

       - Type `exit` to return to the recovery menu.

  6. **Resume Normal Boot**

     - In the recovery menu, select **`resume`** to continue with the normal boot process.
     - The system should now boot normally if `fsck` was able to repair the filesystem.

- *(Linux/macOS)* Alternatively, run `fsck` over a mounted image on the host (see {ref}`launch-start-issues-reading-data-from-an-image`).
- Run `qemu-img check -r` on the image.
    * `qemu-img`, shipped with Multipass, can also be used to check and repair disk images.
    * See {ref}`launch-start-issues-locate-multipass-binaries` below.
    * See {ref}`launch-start-issues-locate-multipass-images` below.
    * For example:
    ```{code-block} text
    /Library/Application\ Support/com.canonical.multipass/bin/qemu-img check -r /var/root/Library/Application\ Support/multipassd/qemu/vault/instances/<instance>/<img>
    ```
- If none of the above works, you can still try to mount the image manually to recover data (see {ref}`launch-start-issues-reading-data-from-an-image`).

(launch-start-issues-no-ip-assigned)=
### No IP assigned

Sometimes VMs request an IP address, but don't obtain one. That can happen because of interference from other software, VPNs, network misconfiguration and, firewall settings.

#### [macOS, QEMU driver] Firewall blocks `bootp`

The macOS firewall is known to cause `vmnet` to malfunction, because it blocks Apple's own `bootp` from giving out IPs. The effect of this problem on Multipass is tracked in [this issue](https://github.com/canonical/multipass/issues/2387), which we internally call the *dreaded firewall issue*.

You may be able to work around it by disabling the firewall entirely, or executing

```{code-block} text
/usr/libexec/ApplicationFirewall/socketfilterfw --add /usr/libexec/bootpd
/usr/libexec/ApplicationFirewall/socketfilterfw --unblock /usr/libexec/bootpd
```

We are aware that this requires administrative privileges, which managed Macs won't have. We unfortunately don't have a better fix for those cases. We continue hoping that Apple will eventually fix the problem which, to the best of our knowledge, affects all products using `vmnet`. Chances of that happening will probably increase if enough people [report it to them](https://developer.apple.com/bug-reporting/).

> See also: [How to troubleshoot networking](/how-to-guides/troubleshoot/troubleshoot-networking).
>
#### [macOS, QEMU driver] `bootp` not coming up

The DHCP server should be launched automatically when there is a request, but you can also launch it manually if needed. To do so, run:

```{code-block} text
sudo launchctl start com.apple.bootpd
```

If that doesn't work for you, try :

```{code-block} text
sudo launchctl load -w /System/Library/LaunchDaemons/bootps.plist
```
(launch-start-issues-stale-sharing-lease)=
### [Windows, with Hyper-V] Stale internet connection sharing lease

Another possible reason for instance timeouts is a problem with the `Internet Connection Sharing` hosts file. This file sometimes gets corrupted, with jumbled or incomplete text. Other times, it contains duplicate or stale IP addresses.

Using Administrator privileges, edit the file `C:\WINDOWS\System32\drivers\etc\hosts.ics` and look for any corruption or entries that have your instance name in it. If there is more than one entry, remove any of them except for the first one listed. Save the file and try again. If that does not work, stop any running instances, delete the file, and reboot.

### SSH issues

If SSH doesn't function properly in the VM, or Multipass is blocked from accessing it, your instance may need to be reconfigured or repaired.

* If the default user is not `ubuntu`, Multipass cannot connect. If you used a custom cloud-init config file, make sure that the default user is `ubuntu`.

* if SSH keys are missing or incorrect, you will have to add your public SSH key from `~/.ssh/id_rsa.pub` on the host to `~/.ssh/authorized_keys` in the instance. To do so you may need to gain access to the instance through a method besides SSH.

To gain access to an instance without SSH you can try the following methods.

* Mount the instance's image file on your host (see {ref}`launch-start-issues-reading-data-from-an-image`) and make necessary changes through the filesystem.

* Run the instance VM directly. This will require a username and password to log in. The username is the default user, `ubuntu`, and the password is what was set in cloud-init if you used a custom cloud-init config. If you do not have a password you can modify the instance's `cloud-init-config.iso` file to change it. One way to do so is as follows.
  1. Back up your existing `cloud-init-config.iso`.
  2. Make a new instance by running `multipass launch --cloud-init config.yaml`, the contents `config.yaml` are shown below.
  3. Replace your existing `cloud-init-config.iso` with the newly generated `cloud-init-config.iso`.
  4. Restart the VM and use the password `ubuntu`. The instance's password will remain `ubuntu` unless it is changed again
  5. Make necessary changes.

```{code-block} text
#cloud-config
password: ubuntu
chpasswd: { expire: false }
```

### Cloud-init tarries during an instance launch

- When launching a new instance, once Multipass obtains an SSH session to the instance, it will wait for cloud-init to complete. During this phase, the CLI/GUI spinner reads "Waiting for initialisation to complete".

- At this point, the initialisation continues in the background, even if you interrupt the launch command or if it times out.

- So if you wait for a little while longer, your instance may eventually finish setting up. When it does, it will have this file: `/var/lib/cloud/instance/boot-finished`.
    * Consider passing a longer timeout to the `launch` command. For example, `multipass launch --timeout 1000` sets the launch timeout to 1000 seconds.

* You can use `multipass shell` to get a shell in the instance when Multipass is waiting for cloud-init to finish. To diagnose problems, inspect cloud-init's logs in `/var/log/cloud-init*log`.

(launch-start-issues-windows-run-virtualbox)=
### [Windows, VirtualBox driver] Running VirtualBox with the system account

To run the VirtualBox GUI with the system account, you will need a Windows tool called PsExec:

1. Install [PsExec](https://docs.microsoft.com/en-us/sysinternals/downloads/psexec).
1. Add it to your `PATH`.
1. Run PowerShell as Administrator.
1. Execute `psexec.exe -s -i "C:\Program Files\Oracle\VirtualBox\VirtualBox.exe"` (adapt the path to the VirtualBox executable as needed).

When successful, you should see Multipass's instances in VirtualBox

(launch-start-issues-stale-network-cache)=
### Stale network cache

This can be caused by a known Qt bug (see issue [#1714](https://github.com/canonical/multipass/issues/1714) on our GitHub).

A workaround to resolve this issue is to run the command `multipass find --force-update`, which forces downloading the image information from the network. As a result, if the download is successful, the `network-cache` will be overwritten.

Alternatively, try deleting the `network-cache` folder and restart the Multipass service:

* *(on Linux)*
   ```{code-block} text
   sudo snap stop multipass
   sudo rm -rf /var/snap/multipass/common/cache/multipassd/network-cache/
   sudo snap start multipass
   ```
* *(on macOS)*
   ```{code-block} text
   sudo launchctl unload /Library/LaunchDaemons/com.canonical.multipassd.plist
   sudo rm -rf /System/Volumes/Data/private/var/root/Library/Caches/multipassd/network-cache
   sudo launchctl load /Library/LaunchDaemons/com.canonical.multipassd.plist
   ```
* *(on Windows)*
   Remove `C:\ProgramData\Multipass\cache\network-cache` and restart the Multipass service.

(launch-start-issues-reading-data-from-an-image)=
### Reading data from a QCOW2 image

The images that Multipass uses with the QEMU driver follow a standard format — QCOW2 — which other tools can read.

One example is [`qemu-nbd`](https://manpages.ubuntu.com/manpages/questing/en/man8/qemu-nbd.8.html), which allows mounting the image. This tool is not shipped with Multipass, so you would need to install it manually.

Once you have it, you can search the web for recipes to mount a QCOW2 image. For example, here is a [a recipe](https://askubuntu.com/a/4404).

(launch-start-issues-locate-multipass-binaries)=
### Locating Multipass binaries
You may need to locate where Multipass is installed. There are several ways to do so, depending on your platform:
* *(on Linux)*
  * Run the command `which multipass` or `whereis multipass`.
  * By default, Multipass is installed in the `/snap/bin` folder.

* *(on Windows)*
  * Run the command `where.exe multipass`.
  * Right-click a shortcut to Multipass in your files or Start menu and select "Open file location".
  * By default, Multipass is installed in the `C:\Program Files\Multipass\bin` folder.

* *(on macOS)*
  * Run the command `readlink -f $(which multipass)`
  * By default, Multipass is installed in the `/Library/Application\ Support/com.canonical.multipass/bin/` folder.

(launch-start-issues-locate-multipass-images)=
### Locating Multipass images
You may need to locate where Multipass is storing instances. The location changes depending on your platform:
* *(Linux)* `/root/.local/share/multipassd/vault/instances/<instance/<img>`

* *(Windows)* `C:\ProgramData\Multipass\data\vault\instances\<instance>\<img>`

* *(macOS)* `/var/root/Library/Application\ Support/multipassd/qemu/vault/instances/<instance>/<img>`
